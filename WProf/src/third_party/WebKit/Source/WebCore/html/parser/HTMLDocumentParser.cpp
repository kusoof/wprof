/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLDocumentParser.h"

#include "ContentSecurityPolicy.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLParserScheduler.h"
#include "HTMLTokenizer.h"
#include "HTMLPreloadScanner.h"
#include "HTMLScriptRunner.h"
#include "HTMLTreeBuilder.h"
#include "HTMLDocument.h"
#include "InspectorInstrumentation.h"
#include "NestingLevelIncrementer.h"
#include "Settings.h"
#include "Logging.h" // Sophia
#include <wtf/CurrentTime.h> // Sophia
#include <wtf/MD5.h> // Sophia
#include <wtf/Vector.h> // Sophia
#include <wtf/text/CString.h> // Sophia

#if !WPROF_DISABLED
#include "Logging.h"
#include "WprofController.h"
#include "WprofGenTag.h"
#include <wtf/CurrentTime.h>
#include <wtf/MD5.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/AtomicString.h>
#endif

namespace WebCore {

using namespace HTMLNames;

namespace {

// This is a direct transcription of step 4 from:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
HTMLTokenizerState::State tokenizerStateForContextElement(Element* contextElement, bool reportErrors)
{
    if (!contextElement)
        return HTMLTokenizerState::DataState;

    const QualifiedName& contextTag = contextElement->tagQName();

    if (contextTag.matches(titleTag) || contextTag.matches(textareaTag))
        return HTMLTokenizerState::RCDATAState;
    if (contextTag.matches(styleTag)
        || contextTag.matches(xmpTag)
        || contextTag.matches(iframeTag)
        || (contextTag.matches(noembedTag) && HTMLTreeBuilder::pluginsEnabled(contextElement->document()->frame()))
        || (contextTag.matches(noscriptTag) && HTMLTreeBuilder::scriptEnabled(contextElement->document()->frame()))
        || contextTag.matches(noframesTag))
        return reportErrors ? HTMLTokenizerState::RAWTEXTState : HTMLTokenizerState::PLAINTEXTState;
    if (contextTag.matches(scriptTag))
        return reportErrors ? HTMLTokenizerState::ScriptDataState : HTMLTokenizerState::PLAINTEXTState;
    if (contextTag.matches(plaintextTag))
        return HTMLTokenizerState::PLAINTEXTState;
    return HTMLTokenizerState::DataState;
}

} // namespace

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument* document, bool reportErrors)
    : ScriptableDocumentParser(document)
    , m_tokenizer(HTMLTokenizer::create(usePreHTML5ParserQuirks(document)))
    , m_scriptRunner(HTMLScriptRunner::create(document, this))
    , m_treeBuilder(HTMLTreeBuilder::create(this, document, reportErrors, usePreHTML5ParserQuirks(document), maximumDOMTreeDepth(document)))
    , m_parserScheduler(HTMLParserScheduler::create(this))
    , m_xssAuditor(this)
    , m_endWasDelayed(false)
    , m_pumpSessionNestingLevel(0)
{
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTMLDocumentParser::HTMLDocumentParser(DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
    : ScriptableDocumentParser(fragment->document())
    , m_tokenizer(HTMLTokenizer::create(usePreHTML5ParserQuirks(fragment->document())))
    , m_treeBuilder(HTMLTreeBuilder::create(this, fragment, contextElement, scriptingPermission, usePreHTML5ParserQuirks(fragment->document()), maximumDOMTreeDepth(fragment->document())))
    , m_xssAuditor(this)
    , m_endWasDelayed(false)
    , m_pumpSessionNestingLevel(0)
{
    bool reportErrors = false; // For now document fragment parsing never reports errors.
    m_tokenizer->setState(tokenizerStateForContextElement(contextElement, reportErrors));
}

HTMLDocumentParser::~HTMLDocumentParser()
{
    ASSERT(!m_parserScheduler);
    ASSERT(!m_pumpSessionNestingLevel);
    ASSERT(!m_preloadScanner);
    ASSERT(!m_insertionPreloadScanner);
}

void HTMLDocumentParser::detach()
{
	//LOG(DependencyLog, "HTMLDocumentParser.cpp::detach"); // Sophia
    DocumentParser::detach();
    if (m_scriptRunner)
        m_scriptRunner->detach();
    m_treeBuilder->detach();
    // FIXME: It seems wrong that we would have a preload scanner here.
    // Yet during fast/dom/HTMLScriptElement/script-load-events.html we do.
    m_preloadScanner.clear();
    m_insertionPreloadScanner.clear();
    m_parserScheduler.clear(); // Deleting the scheduler will clear any timers.
}

void HTMLDocumentParser::stopParsing()
{
    DocumentParser::stopParsing();
    m_parserScheduler.clear(); // Deleting the scheduler will clear any timers.
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::prepareToStopParsing()
{
    ASSERT(!hasInsertionPoint());

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);
	
	//LOG(DependencyLog, "HTMLDocumentParser.cpp::prepareToStopParsing"); // Sophia
    // NOTE: This pump should only ever emit buffered character tokens,
    // so ForceSynchronous vs. AllowYield should be meaningless.
    pumpTokenizerIfPossible(ForceSynchronous);
    
    if (isStopped())
        return;

    DocumentParser::prepareToStopParsing();

    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (m_scriptRunner)
        document()->setReadyState(Document::Interactive);

    attemptToRunDeferredScriptsAndEnd();
}

bool HTMLDocumentParser::isParsingFragment() const
{
    return m_treeBuilder->isParsingFragment();
}

bool HTMLDocumentParser::processingData() const
{
    return isScheduledForResume() || inPumpSession();
}

void HTMLDocumentParser::pumpTokenizerIfPossible(SynchronousMode mode)
{

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::pumpTokenizerIfPossible %p", this);
#endif

	//LOG(DependencyLog, "HTMLDocumentParser.cpp::pumpTokenizerIfPossible %p", this); // Sophia
    if (isStopped() || m_treeBuilder->isPaused())
        return;

    // Once a resume is scheduled, HTMLParserScheduler controls when we next pump.
    if (isScheduledForResume()) {
        ASSERT(mode == AllowYield);
        return;
    }

    pumpTokenizer(mode);
}

bool HTMLDocumentParser::isScheduledForResume() const
{
    return m_parserScheduler && m_parserScheduler->isScheduledForResume();
}

// Used by HTMLParserScheduler
void HTMLDocumentParser::resumeParsingAfterYield()
{

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::resumeParsingAfterYield %p", this);
#endif

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    // We should never be here unless we can pump immediately.  Call pumpTokenizer()
    // directly so that ASSERTS will fire if we're wrong.
    pumpTokenizer(AllowYield);
    endIfDelayed();
}

bool HTMLDocumentParser::runScriptsForPausedTreeBuilder()
{
    ASSERT(m_treeBuilder->isPaused());

    TextPosition scriptStartPosition = TextPosition::belowRangePosition();
    RefPtr<Element> scriptElement = m_treeBuilder->takeScriptToProcess(scriptStartPosition);
    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (!m_scriptRunner)
        return true;

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::runScriptsForPausedTreeBuilder %p", this);
#endif

    return m_scriptRunner->execute(scriptElement.release(), scriptStartPosition);
}

bool HTMLDocumentParser::canTakeNextToken(SynchronousMode mode, PumpSession& session)
{
    if (isStopped())
        return false;

    // The parser will pause itself when waiting on a script to load or run.
    if (m_treeBuilder->isPaused()) {
        if (mode == AllowYield)
            m_parserScheduler->checkForYieldBeforeScript(session);

        // If we don't run the script, we cannot allow the next token to be taken.
        if (session.needsYield)
            return false;

        // If we're paused waiting for a script, we try to execute scripts before continuing.
        bool shouldContinueParsing = runScriptsForPausedTreeBuilder();
        m_treeBuilder->setPaused(!shouldContinueParsing);
        if (!shouldContinueParsing || isStopped())
            return false;
    }

    // FIXME: It's wrong for the HTMLDocumentParser to reach back to the
    //        Frame, but this approach is how the old parser handled
    //        stopping when the page assigns window.location.  What really
    //        should happen is that assigning window.location causes the
    //        parser to stop parsing cleanly.  The problem is we're not
    //        perpared to do that at every point where we run JavaScript.
    if (!isParsingFragment()
        && document()->frame() && document()->frame()->navigationScheduler()->locationChangePending())
        return false;

    if (mode == AllowYield)
        m_parserScheduler->checkForYieldBeforeToken(session);

    return true;
}

void HTMLDocumentParser::pumpTokenizer(SynchronousMode mode)
{
#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::pumpTokenizer");
#endif

    ASSERT(!isStopped());
    ASSERT(!isScheduledForResume());
    // ASSERT that this object is both attached to the Document and protected.
    ASSERT(refCount() >= 2);

    PumpSession session(m_pumpSessionNestingLevel);

    // We tell the InspectorInstrumentation about every pump, even if we
    // end up pumping nothing.  It can filter out empty pumps itself.
    // FIXME: m_input.current().length() is only accurate if we
    // end up parsing the whole buffer in this pump.  We should pass how
    // much we parsed as part of didWriteHTML instead of willWriteHTML.
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willWriteHTML(document(), m_input.current().length(), m_tokenizer->lineNumber().zeroBasedInt());

    while (canTakeNextToken(mode, session) && !session.needsYield) {

#if !WPROF_DISABLED
      double startTime = monotonicallyIncreasingTime();
#endif
      
      int previousCharactersConsumed = m_input.current().numberOfCharactersConsumed();
      //fprintf(stderr, "Before the next token for line %d, the value is %d\n", m_input.current().currentLine(), previousCharactersConsumed);
        if (!isParsingFragment())
            m_sourceTracker.start(m_input, m_tokenizer.get(), m_token);

        if (!m_tokenizer->nextToken(m_input.current(), m_token))
            break;

/*#if !WPROF_DISABLED
      //Get the token name, token type, and the text position
      //We need to do this before we parse the token.
	HTMLTokenTypes::Type tokenType = m_token.type();
	AtomicString tokenName;
	if((tokenType == HTMLTokenTypes::StartTag) || (tokenType == HTMLTokenTypes::EndTag)) {
	  tokenName = AtomicString(m_token.name().data(), m_token.name().size());
	}
	TextPosition position = textPosition();
#endif*/

        if (!isParsingFragment()) {
            m_sourceTracker.end(m_input, m_tokenizer.get(), m_token);

            // We do not XSS filter innerHTML, which means we (intentionally) fail
            // http/tests/security/xssAuditor/dom-write-innerHTML.html
            m_xssAuditor.filterToken(m_token);
        }

#if !WPROF_DISABLED
	int haveConsumed = m_input.current().numberOfCharactersConsumed() - previousCharactersConsumed;
        LOG(DependencyLog, "HTMLDocumentParser.cpp::pumpTokenizer charConsumed %d %d", m_input.current().numberOfCharactersConsumed(), m_input.current().length());

	if(!isParsingFragment()){
	  WprofController::getInstance()->addCharactersConsumed(haveConsumed, document(), m_input.current().currentLine().zeroBasedInt());
	}
	else{
	  //Get the document fragment
	  DocumentFragment* fragment = m_treeBuilder->fragment();
	  WprofController::getInstance()->addCharactersConsumed(haveConsumed, fragment, m_input.current().currentLine().zeroBasedInt());
	}
#endif

        m_treeBuilder->constructTreeFromToken(m_token);
        ASSERT(m_token.isUninitialized());

#if !WPROF_DISABLED
     double endTime = monotonicallyIncreasingTime();

     Document* doc = document();
     Page* page = WprofController::getInstance()->getPageFromDocument(doc);

     if(page){   
       WprofElement* element = WprofController::getInstance()->tempElementForPage(page);
       if(element && element->startTime() == 0){ //make sure it hasn't been set before.
	 element->setStartEndTime(startTime, endTime);
       }
     }
#endif
    }

    // Ensure we haven't been totally deref'ed after pumping. Any caller of this
    // function should be holding a RefPtr to this to ensure we weren't deleted.
    ASSERT(refCount() >= 1);

    if (isStopped())
        return;

    if (session.needsYield)
        m_parserScheduler->scheduleForResume();

    if (isWaitingForScripts()) {
        ASSERT(m_tokenizer->state() == HTMLTokenizerState::DataState);
        if (!m_preloadScanner) {
            m_preloadScanner = adoptPtr(new HTMLPreloadScanner(document()));
            m_preloadScanner->appendToEnd(m_input.current(), 0);
        }
        m_preloadScanner->scan();
    }

    InspectorInstrumentation::didWriteHTML(cookie, m_tokenizer->lineNumber().zeroBasedInt());
}

bool HTMLDocumentParser::hasInsertionPoint()
{
    // FIXME: The wasCreatedByScript() branch here might not be fully correct.
    //        Our model of the EOF character differs slightly from the one in
    //        the spec because our treatment is uniform between network-sourced
    //        and script-sourced input streams whereas the spec treats them
    //        differently.
    return m_input.hasInsertionPoint() || (wasCreatedByScript() && !m_input.haveSeenEndOfFile());
}

void HTMLDocumentParser::insert(const SegmentedString& source)
{
    if (isStopped())
        return;

#if !WPROF_DISABLED
    HTMLDocumentParser* parser = (HTMLDocumentParser*)(document()->parser());
    if (parser != NULL) {
        LOG(DependencyResults, "HTMLDocumentParser.cpp::insert (Parser %p) INFO (StartParsing %lf)", this, monotonicallyIncreasingTime());
    }
#endif

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    SegmentedString excludedLineNumberSource(source);
    excludedLineNumberSource.setExcludeLineNumbers();
    m_input.insertAtCurrentInsertionPoint(excludedLineNumberSource);
    pumpTokenizerIfPossible(ForceSynchronous);
    
    if (isWaitingForScripts()) {
        // Check the document.write() output with a separate preload scanner as
        // the main scanner can't deal with insertions.
        if (!m_insertionPreloadScanner)
            m_insertionPreloadScanner = adoptPtr(new HTMLPreloadScanner(document()));
        m_insertionPreloadScanner->appendToEnd(source, m_input.current().currentLine().zeroBasedInt());
        m_insertionPreloadScanner->scan();
    }

    endIfDelayed();
}

void HTMLDocumentParser::append(const SegmentedString& source)
{
    if (isStopped())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    if (m_preloadScanner) {
        if (m_input.current().isEmpty() && !isWaitingForScripts()) {
            // We have parsed until the end of the current input and so are now moving ahead of the preload scanner.
            // Clear the scanner so we know to scan starting from the current input point if we block again.
            m_preloadScanner.clear();
        } else {
	  m_preloadScanner->appendToEnd(source, m_input.current().currentLine().zeroBasedInt());
            if (isWaitingForScripts())
                m_preloadScanner->scan();
        }
    }

    m_input.appendToEnd(source);

    if (inPumpSession()) {
        // We've gotten data off the network in a nested write.
        // We don't want to consume any more of the input stream now.  Do
        // not worry.  We'll consume this data in a less-nested write().
        return;
    }

#if !WPROF_DISABLED
    MD5 md5;
    md5.addBytes(reinterpret_cast<const uint8_t*>(source.toString().utf8().data()), source.toString().length());
    Vector<uint8_t, 16> digest;
    md5.checksum(digest);
    char* buf = 0;
    CString actual = CString::newUninitialized(32, buf);
    for (size_t i = 0; i < 16; i++) {
        snprintf(buf, 3, "%02x", digest.at(i));
        buf += 2;
    }

    LOG(DependencyResults, "HTMLDocumentParser.cpp::append (Parser %p) INFO (StartParsing %lf) %s", this, monotonicallyIncreasingTime(), actual.data());
#endif

    pumpTokenizerIfPossible(AllowYield);

    endIfDelayed();
}

void HTMLDocumentParser::end()
{
    ASSERT(!isDetached());
    ASSERT(!isScheduledForResume());

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::end (Parser %p) INFO (FinishParsing %lf)", this, monotonicallyIncreasingTime());
#endif

    // Informs the the rest of WebCore that parsing is really finished (and deletes this).
    m_treeBuilder->finished();
}

void HTMLDocumentParser::attemptToRunDeferredScriptsAndEnd()
{
    ASSERT(isStopping());
    ASSERT(!hasInsertionPoint());
#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::attemptToRunDeferredScriptsAndEnd ThreadId:%d", currentThread());
#endif

    if (m_scriptRunner && !m_scriptRunner->executeScriptsWaitingForParsing())
        return;
    end();
}

void HTMLDocumentParser::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (shouldDelayEnd()) {
        m_endWasDelayed = true;
        return;
    }
    prepareToStopParsing();
}

void HTMLDocumentParser::endIfDelayed()
{
    // If we've already been detached, don't bother ending.
    if (isDetached())
        return;

    if (!m_endWasDelayed || shouldDelayEnd())
        return;

    m_endWasDelayed = false;
    prepareToStopParsing();
}

void HTMLDocumentParser::finish()
{
    // FIXME: We should ASSERT(!m_parserStopped) here, since it does not
    // makes sense to call any methods on DocumentParser once it's been stopped.
    // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

    // We're not going to get any more data off the network, so we tell the
    // input stream we've reached the end of file.  finish() can be called more
    // than once, if the first time does not call end().
    if (!m_input.haveSeenEndOfFile())
        m_input.markEndOfFile();
    attemptToEnd();
}

bool HTMLDocumentParser::finishWasCalled()
{
    return m_input.haveSeenEndOfFile();
}

bool HTMLDocumentParser::isExecutingScript() const
{
    if (!m_scriptRunner)
        return false;
    return m_scriptRunner->isExecutingScript();
}

String HTMLDocumentParser::sourceForToken(const HTMLToken& token)
{
    return m_sourceTracker.sourceForToken(token);
}

OrdinalNumber HTMLDocumentParser::lineNumber() const
{
    return m_tokenizer->lineNumber();
}

TextPosition HTMLDocumentParser::textPosition() const
{
    const SegmentedString& currentString = m_input.current();
    OrdinalNumber line = currentString.currentLine();
    OrdinalNumber column = currentString.currentColumn();
    ASSERT(m_tokenizer->lineNumber() == line);

    return TextPosition(line, column);
}

bool HTMLDocumentParser::isWaitingForScripts() const
{
    return m_treeBuilder->isPaused();
}

void HTMLDocumentParser::resumeParsingAfterScriptExecution()
{
#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::resumeParsingAfterScriptExecution ThreadId:%d %p (Resume %lf)", currentThread(), this, monotonicallyIncreasingTime());
#endif

    ASSERT(!isExecutingScript());
    ASSERT(!m_treeBuilder->isPaused());

    m_insertionPreloadScanner.clear();
    pumpTokenizerIfPossible(AllowYield);
    endIfDelayed();
}

void HTMLDocumentParser::watchForLoad(CachedResource* cachedScript)
{
    ASSERT(!cachedScript->isLoaded());
    // addClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded CachedResource.
    cachedScript->addClient(this);
}

void HTMLDocumentParser::stopWatchingForLoad(CachedResource* cachedScript)
{
    cachedScript->removeClient(this);
}
    
void HTMLDocumentParser::appendCurrentInputStreamToPreloadScannerAndScan()
{
    ASSERT(m_preloadScanner);
    m_preloadScanner->appendToEnd(m_input.current(), 0);
    m_preloadScanner->scan();
}

void HTMLDocumentParser::notifyFinished(CachedResource* cachedResource)
{
#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::notifyFinished %p (Doc %p) (Parser %p)", this, document(), document()->parser());
#endif

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    ASSERT(m_scriptRunner);
    ASSERT(!isExecutingScript());
    if (isStopping()) {
        attemptToRunDeferredScriptsAndEnd();
        return;
    }

    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

void HTMLDocumentParser::executeScriptsWaitingForStylesheets()
{

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::executeScriptsWaitingForStylesheets ThreadId:%d", currentThread());
#endif

    // Document only calls this when the Document owns the DocumentParser
    // so this will not be called in the DocumentFragment case.
    ASSERT(m_scriptRunner);
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForStylesheets())
        return;

    // pumpTokenizer can cause this parser to be detached from the Document,
    // but we need to ensure it isn't deleted yet.
    RefPtr<HTMLDocumentParser> protect(this);

    ASSERT(!m_scriptRunner->isExecutingScript());
    ASSERT(m_treeBuilder->isPaused());
    // Note: We only ever wait on one script at a time, so we always know this
    // is the one we were waiting on and can un-pause the tree builder.
    m_treeBuilder->setPaused(false);
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

ScriptController* HTMLDocumentParser::script() const
{
    return document()->frame() ? document()->frame()->script() : 0;
}

void HTMLDocumentParser::parseDocumentFragment(const String& source, DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
{
    RefPtr<HTMLDocumentParser> parser = HTMLDocumentParser::create(fragment, contextElement, scriptingPermission);

#if !WPROF_DISABLED
    LOG(DependencyLog, "HTMLDocumentParser.cpp::parseDocumentFragment ThreadId:%d", currentThread());
#endif

    parser->insert(source); // Use insert() so that the parser will not yield.
    parser->finish();
    ASSERT(!parser->processingData()); // Make sure we're done. <rdar://problem/3963151>
    parser->detach(); // Allows ~DocumentParser to assert it was detached before destruction.
}
    
bool HTMLDocumentParser::usePreHTML5ParserQuirks(Document* document)
{
    ASSERT(document);
    return document->settings() && document->settings()->usePreHTML5ParserQuirks();
}

unsigned HTMLDocumentParser::maximumDOMTreeDepth(Document* document)
{
    ASSERT(document);
    return document->settings() ? document->settings()->maximumHTMLParserDOMTreeDepth() : Settings::defaultMaximumHTMLParserDOMTreeDepth;
}

void HTMLDocumentParser::suspendScheduledTasks()
{
    if (m_parserScheduler)
        m_parserScheduler->suspend();
}

void HTMLDocumentParser::resumeScheduledTasks()
{
    if (m_parserScheduler)
        m_parserScheduler->resume();
}

}
