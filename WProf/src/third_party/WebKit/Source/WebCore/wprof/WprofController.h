/*
 * WprofController.h
 *
 * WProf is licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php
 *
 * Copyright (c) 2012 University of Washington. All rights reserved
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in the 
 * Software without restriction, including without limitation the rights to use, copy, 
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, subject to the 
 * following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies 
 * or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef WprofController_h
#define WprofController_h

#if !WPROF_DISABLED

#include "Logging.h"
#include "ResourceLoadTiming.h"
#include "WprofConstants.h"
#include "WprofComputation.h"
#include "WprofHTMLTag.h"
#include "WprofPreload.h"
#include "WprofReceivedChunk.h"
#include "WprofResource.h"
#include <wtf/HashMap.h>
#include <wtf/CurrentTime.h>
#include <wtf/MD5.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <Document.h>
#include <DocumentFragment.h>
#include <ResourceRequest.h>
#include <Event.h>

namespace WebCore {

  class ResourceRequest;

typedef struct CurrentPosition {
  int position;
  int lastSeenRow;
  CurrentPosition(int p, int l){
    position = p;
    lastSeenRow = l;
  }
} CurrentPosition;
    
class WprofController {
    public:
	/*
	 * Use Singleton to update and fetch Wprof data
	 * Note that the notion of using Singleton is problematic if multiple pages
	 * are loaded in a single process.
	 */
        static WprofController* getInstance() {
            static WprofController* m_pInstance;
            if (!m_pInstance)
                m_pInstance = new WprofController();
            return m_pInstance;
        };
        
        ~WprofController() {};
        
        /*
         * This function creates a WprofResource object.
         * Called by ResourceLoader::didReceiveResponse().
         * To save memory, WprofResource merges the <url, request time> mapping.
         *
         * @param
         */
        void createWprofResource(
            unsigned long id,
            ResourceRequest& request,
            RefPtr<ResourceLoadTiming> resourceLoadTiming,
            String mime,
            long long expectedContentLength,
            int httpStatusCode,
            unsigned connectionId,
            bool connectionReused,
            bool wasCached) {

	    //Get the URL from the request
	    String url = request.url().string();
	    
            // Find request start time
            double time = getTimeAndRemoveMapping(url);
            
	    //First check whether we are in the middle of a fired event, i.e. the resource
	    //was requested due to a DOMContentLoaded, or load event

	    unsigned long from = 0;
	    WprofResourceFromType fromType = WprofFromTypeInvalid;
	    
	    if(request.wprofComputation()){
	      from = (unsigned long)request.wprofComputation();
	      fromType = WprofFromTypeComp;
	    }
            // Find the WprofHTMLTag that this resource is requested from if any
            // Note that image requested from CSS is considered later
	    // WprofHTMLTag* tag = getRequestWprofHTMLTagByUrl(request);
	    else if(request.wprofHTMLTag()) {
		from  = (unsigned long) request.wprofHTMLTag();
		fromType = WprofFromTypeTag;
	      }

	    // [Note] deep copy request url
	      WprofResource* resource = new WprofResource(id, url, resourceLoadTiming, mime, expectedContentLength, httpStatusCode, connectionId, connectionReused, wasCached, time, from, fromType);
            
            // Add to both Vector and HashMap
            m_wprofResourceVector->append(resource);
            m_wprofResourceMap->set(resource->getId(), resource);

	    //fprintf(stderr, "creating resource with url %s\n", url.utf8().data());
	    //Check if we are in the waiting for last resource state
	    //and we have downloaded all the pending resources
	    if(hasPageLoaded())
	    {
		setPageLoadComplete();
	    }
        }

        /*
         * This function creates a WprofReceivedChunk object.
         * Called by ResourceLoader::didReceiveData().
         *
         * @param unsigned long id of the corresponding request
         * @param unsigned long length of the received chunk
         */
        void createWprofReceivedChunk(unsigned long id_url, unsigned long length) {
	    LOG(DependencyLog, "WprofController::addWprofReceivedChunk %lu", id_url);

	    WprofReceivedChunk* chunk = new WprofReceivedChunk(id_url, length, monotonicallyIncreasingTime());
	    WprofResource* resource = m_wprofResourceMap->get(id_url);
	    if(resource){
	      resource->appendWprofReceivedChunk(chunk);
	      resource->addBytes(length);
	    }
        }
        
        /*
         * This function adds a <url, request time> mapping.
         * Called by ResourceLoader::willSendRequest().
         * To save memory, especially that of the url, we should move this info to WprofResource
         * a corresponding WprofResource object is created.
         *
         * @param const char request url
         */
        void createRequestTimeMapping(String url) {
            m_requestTimeMap->set(url, monotonicallyIncreasingTime());
        }

	void matchWithPreload(WprofHTMLTag* tag){
	  Vector<WprofPreload*>::iterator it = m_unmatchedPreloads->begin();
	   size_t position = 0;
	   TextPosition textPosition = tag->pos();
	   for(; it != m_unmatchedPreloads->end(); it++){
	     //while(position < m_unmatchedPreloads->size()){
	     //WprofPreload* preload = (*m_unmatchedPreloads)[position];
	     if((*it)->matchesToken(tag->docUrl(), tag->urls(), tag->tagName(), textPosition.m_line.zeroBasedInt(), textPosition.m_column.zeroBasedInt())){
	       (*it)->setFromTag(tag);
		m_unmatchedPreloads->remove(position);
		break;
	      }
	     //else{
		position++;
	     //}
	    }
	}

        /*
         * This function creates a WprofHTMLTag object.
         * Called by HTMLTreeBuilder::constructTreeFromToken().
         *
         * @param TextPosition textPosition of the tag
         * @param String url of the document
         * @param String token of the tag
         * @param bool whether this is a start tag
         */
        void createWprofHTMLTag(
            TextPosition textPosition,
	    String docUrl,
	    Document* document,
            String token,
            bool isStartTag) {

	  /*fprintf(stderr, "Creating new tag for document pointer %p, docurl  %s, line number %d, column %d tag %s, chars consumed %d\n",
		  document, docUrl.utf8().data(), textPosition.m_line.zeroBasedInt(), textPosition.m_column.zeroBasedInt(),
		  token.utf8().data(), charConsumed());*/

	  //Get the entry corresponding to the doc url
	  CurrentPosition* charPos = m_documentCurrentPositionMap->get(document);

	  bool isFragment = false;

	  Document* ownerDocument = document->parentDocument();

	  if(ownerDocument){
	    //fprintf(stderr, "Document with url %s has an owner with url %s\n",
	    //	    docUrl.utf8().data(), ownerDocument->url().string().utf8().data());

	    //If the URLs of the owner and document are the same, then we have a fragment
	    if(ownerDocument->url().string() == docUrl) isFragment = true;
	  }

	  /*if(charPos->lastSeenRow > textPosition.m_line.zeroBasedInt()){
	    fragment = true;
	    }*/

            WprofHTMLTag* tag = new WprofHTMLTag(
                textPosition,
                docUrl,
                token,
                charPos->position,
                charLen(),
		isFragment,
                isStartTag
            );

	    /*fprintf(stderr, "Creating new tag for fragment with document  pointer %p, docurl  %s, line number %d, column %d tag %s, chars consumed %d\n",
		  document, docUrl.utf8().data(), textPosition.m_line.zeroBasedInt(), textPosition.m_column.zeroBasedInt(),
		  token.utf8().data(), charConsumed());*/

            // Add WprofHTMLTag to its vector
            appendDerivedWprofHTMLTag(tag);

            setTempWprofHTMLTag(tag);

            if (isStartTag) {
                // Add tag to the start vector
                addElementStart(tag);

                if (token == String::format("script")) {
                    // Add tag to tempWprofHTMLTag for EndTag
                    // This works because there is no children inside elements
                    // we care about (<script>, <link> and <style>)

                    // Set type as normal. We will change it if async or defer
                    setElementTypePair(tag, 1); // normal
                }
            }
        }

	//Wprof tags created from parsing a document fragment.
	void createWprofHTMLTag(
            TextPosition textPosition,
	    String docUrl,
	    DocumentFragment* fragment,
            String token,
            bool isStartTag) {

	  //Get the entry corresponding to the fragment
	  CurrentPosition* charPos = m_fragmentCurrentPositionMap->get(fragment);

            WprofHTMLTag* tag = new WprofHTMLTag(
                textPosition,
                docUrl,
                token,
                charPos->position,
                charLen(),
		true,
                isStartTag
	    );

            // Add WprofHTMLTag to its vector
            appendDerivedWprofHTMLTag(tag);

            setTempWprofHTMLTag(tag);

	    if (isStartTag) {
                // Add tag to the start vector
                addElementStart(tag);

                if (token == String::format("script")) {
                    // Add tag to tempWprofHTMLTag for EndTag
                    // This works because there is no children inside elements
                    // we care about (<script>, <link> and <style>)

                    // Set type as normal. We will change it if async or defer
                    setElementTypePair(tag, 1); // normal
                }
            }
        }

        /*
         * Create a WprofComputation object.
         * Called by TODO
         *
         * @param int type of the WprofComputation
         */
        WprofComputation* createWprofComputation(int type) {
            WprofComputation* event = new WprofComputation(type, tempWprofHTMLTag());
            m_wprofComputationVector->append(event);

            return event;
        }

	WprofComputation* createWprofComputation(int type, WprofHTMLTag* tag){

	  if(!tag){
	    tag = tempWprofHTMLTag();
	  }

	  WprofComputation* event = new WprofComputation(type, tag);
          m_wprofComputationVector->append(event);

          return event;
	}

	void willFireEventListeners(Event* event)
	{
	  //fprintf(stderr, "Will fire event of type %s\n", event->type().string().utf8().data());
	  m_currentEvent = event;
	}

	void didFireEventListeners()
	{
	  m_currentEvent = NULL;
	}


	/* ------------------------------- ----------------------- 
	                              Timers
	----------------------------------------------------------*/
	
	void installTimer(int timerId, int timeout, bool singleShot)
	{
	  m_timers->append(timerId);
	  //fprintf(stderr, "adding timer with id %d\n", timerId);
	}

	void removeTimer(int timerId)
	{
	  for(size_t i = 0; i != m_timers->size(); i++){
	    if((*m_timers)[i] == timerId){
	      m_timers->remove(i);
	      break;
	    }
	  }

	  //Check if we can complete the page load
	  if(hasPageLoaded()){
	    setPageLoadComplete();
	  }
	}

	WprofComputation*  willFireTimer(int timerId)
	{
	  WprofComputation* comp = createWprofComputation(5);
	  comp->setUrlRecalcStyle("Timer");
	  return comp;
	}

	void didFireTimer(int timerId, WprofComputation* comp)
	{
	  comp->end();
	  //fprintf(stderr, "timer did fire with id %d\n", timerId);
	  removeTimer(timerId);
	}

        /*
         * Create a WprofPreload object.
         * Called by HTMLPreloadScanner::preload().
         *
         * @param String the preloaded url
         */
        void createWprofPreload(String url, String docUrl, String tagName, int line, int column) {
	  WprofPreload* preload = new WprofPreload(tempWprofHTMLTag(), url, docUrl, tagName, line, column);
	  /*fprintf(stderr,"creating a preload with docurl %s, url %s, tagname %s, line %d, column %d\n",
	    docUrl.utf8().data(), url.utf8().data(), tagName.utf8().data(), line, column);*/
            m_wprofPreloadVector->append(preload);
	    m_unmatchedPreloads->append(preload);
        }
        
        // CSS -> Image doesn't need this because this kind of dependency is
        // inferred by text matching
        void createRequestWprofHTMLTagMapping(ResourceRequest& request, WprofHTMLTag* tag) {
	  String url = request.url().string();
	  	  
	  //First check if we are in the middle of executing an event, if so, then the resource
	  //download will depend on the event computation not an HTML tag
	  if(m_currentEvent && ((m_currentEvent->type().string() == String::format("load")) 
				|| (m_currentEvent->type().string() == String::format("DOMContentLoaded")))){
	    WprofComputation* comp = getWprofComputation();
	    if(comp && (comp->type() == 5)){
	      request.setWprofComputation(getWprofComputation());
	      //fprintf(stderr, "we are attempting to send a request during en event, set the computation in the request\n");
	    }
	  }
	  //Assign the wproftag to the request
	  //This could be the very first request for the page, which has a null tag.
	  else if(tag){
	    request.setWprofHTMLTag(tag);
	  
	    //Add the url to the list
	    tag->appendUrl(url);
	    
	    matchWithPreload(tag);
	  }
	  /*else{
	    fprintf(stderr, "matching request to tag, and the tag is null \n");
	    }*/

	    /*fprintf(stderr, "adding url %s to tag with name %s line %d column %d\n", url.utf8().data(),
	      tag->tagName().utf8().data(), tag->pos().m_line.zeroBasedInt(), tag->pos().m_column.zeroBasedInt());*/
        }
        
        void createRequestWprofHTMLTagMapping(ResourceRequest& request) {
	  createRequestWprofHTMLTagMapping(request, tempWprofHTMLTag());
        }

        void addElementStart(WprofHTMLTag* tag) {
            m_elemStartVector->append(tag);
        }
        
        void setElementTypePair(WprofHTMLTag* key, int value) {
            if (key == NULL)
                return;
            
            // Should check whether key has existed
            HashMap<WprofHTMLTag*, int>::iterator iter = m_elemTypeMap->begin();
            for (; iter != m_elemTypeMap->end(); ++iter) {
                if (*key == *iter->first) {
                    m_elemTypeMap->set(iter->first, value);
                    return;
                }
            }
            m_elemTypeMap->set(key, value);
        }
        
        // For temporarily stored obj hash
        WprofHTMLTag* tempWprofHTMLTag() { return m_tempWprofHTMLTag; }
        
        String HTMLLinkRecalcStyle() { return m_HTMLLinkRecalcStyle; }
        void setHTMLLinkRecalcStyle(String url) {
            m_HTMLLinkRecalcStyle = url;
        }
        
        int charConsumed() { return m_charConsumed; }
        int charLen() { return m_charLen; }
        void setCharConsumed(int charConsumed, int charLen) {
	  //m_charConsumed = charConsumed;
            m_charLen = charLen;
        }

	void addCharactersConsumed(int numberChars, Document* document, int row){
	  //First get the entry in the map, corresponding to the document
	  if(!m_documentCurrentPositionMap->contains(document)){
	    //fprintf(stderr, "adding new document with pointer %p\n", document);
	    CurrentPosition* charPos = new CurrentPosition(numberChars, row);
	    m_documentCurrentPositionMap->set(document, charPos);
	  }
	  else{
	    CurrentPosition* charPos = m_documentCurrentPositionMap->get(document);
	    if(charPos->lastSeenRow <= row){
	      charPos->position += numberChars;
	      charPos->lastSeenRow = row;
	    }
	  }
	  m_charConsumed+= numberChars;
	}

	void addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row){
	  //First get the entry in the map, corresponding to the fragment
	  if(!m_fragmentCurrentPositionMap->contains(fragment)){
	    //fprintf(stderr, "adding new document with pointer %p\n", document);
	    CurrentPosition* charPos = new CurrentPosition(numberChars, row);
	    m_fragmentCurrentPositionMap->set(fragment, charPos);
	  }
	  else{
	    CurrentPosition* charPos = m_fragmentCurrentPositionMap->get(fragment);
	    if(charPos->lastSeenRow <= row){
	      charPos->position += numberChars;
	      charPos->lastSeenRow = row;
	    }
	  }
	  m_charConsumed+= numberChars;
	}
        
        // ---------------------------------------------------------------
        // Methods for CSS elements (images could be requested from CSS)
        
        void addCSSUrl(String url) {
            // We make a hard copy of the url here so that we don't need to hard copy outside
            m_cssUrlVector->append(url);
        }
        
        void clearCSSUrlVector() {
            m_cssUrlVector->clear();
            ASSERT(m_cssUrlVector->isEmpty());
        }
        
        void setCSSContentPair(WprofHTMLTag* key, String value) {
	  if (key == NULL || value.isEmpty())
                return;
            
            // Should check whether key has existed
            HashMap<WprofHTMLTag*, String>::iterator iter = m_cssContentMap->begin();
            for (; iter != m_cssContentMap->end(); ++iter) {
                if (*key == *iter->first) {
                    m_cssContentMap->set(iter->first, value);
                    return;
                }
            }
            m_cssContentMap->set(key, value);
        }
        
        // ---------------------------------------------------------------
        // Methods for computational events
        WprofComputation* getWprofComputation() {
            return m_wprofComputationVector->last();
        }

        // ---------------------------------------------------------------
        // Output methods
        // ---------------------------------------------------------------
        
        // Increase/decrease DOM counter, called in Document.cpp
        void increaseDomCounter() { m_domCounter++; }

	void setWindowLoadEventFired(String url)
	{
	  if (m_state == WPROF_BEGIN){
	    m_state = WPROF_WAITING_LAST_RESOURCE;

	    //if the pending request list is already empty, call the complete event
	    if(hasPageLoaded()){
	      setDocumentURL(url);
	      setPageLoadComplete();
	    }

	  } 
	}

	bool hasPageLoaded (){
	  return (m_state == WPROF_WAITING_LAST_RESOURCE) && (m_requestTimeMap->isEmpty()) 
	    && (m_timers->isEmpty()) && (m_domCounter <=0);
	}
        
        void decreaseDomCounter(String url) {
            m_domCounter--;

	    if(hasPageLoaded()){
	      setDocumentURL(url);
	      setPageLoadComplete();
	    }
            
            /*if (m_domCounter > 0) {
		return;
	    }

            if (url.length() < 5) {
                clear();
                return;
	    }
                
	    if (!url.startsWith("http")) {
                clear();
                return;
            }
                
            // Now we should have doc that only begins with http
	    StringBuilder stringBuilder;

	    m_url = createFilename(url);
	    m_uid = m_url + String::number(monotonicallyIncreasingTime());

	    //LOG(DependencyLog, "WprofController::decreaseCounter %s", m_url.c_str());
                
            // Clear maps
            outputAndClear();
	    clear();*/
        }

	void setPageLoadComplete() {
	  // Clear maps
	  outputAndClear();
	  clear();
	  m_state = WPROF_BEGIN;
        }

	bool setDocumentURL(String url)
	{
	  /*if (!url.startsWith("http")) {
	    clear();
	    m_state = WPROF_BEGIN;
	    return false;
	    }*/
                
	  // Now we should have doc that only begins with http
	  StringBuilder stringBuilder;

	  m_url = createFilename(url);
	  m_uid = m_url + String::number(monotonicallyIncreasingTime());
	  return true;
	}
        
private:
        WprofController()
		: m_cssContentMap(NULL)
		, m_tempWprofHTMLTag(NULL)
	        , m_charConsumed(0)
		, m_charLen(0)
	        , m_HTMLLinkRecalcStyle(emptyString())
		, m_domCounter(0)
         	, m_url(emptyString())
	  , m_state (WPROF_BEGIN)
        {
            m_wprofResourceVector = new Vector<WprofResource*>;
            m_wprofResourceMap = new HashMap<unsigned long, WprofResource*>();
            m_requestTimeMap = new HashMap<String, double>();
            //m_requestWprofHTMLTagMap = new HashMap<ResourceRequest, WprofHTMLTag*>();

	    //Initialize the html tag position maps for documents and fragments 
	    m_documentCurrentPositionMap = new HashMap<Document*, CurrentPosition*>();
	    m_fragmentCurrentPositionMap = new HashMap<DocumentFragment*, CurrentPosition*>();

	    m_unmatchedPreloads = new Vector<WprofPreload*>();
            
            m_elemStartVector = new Vector<WprofHTMLTag*>;
            m_elemTypeMap = new HashMap<WprofHTMLTag*, int>();
            
            m_cssContentMap = new HashMap<WprofHTMLTag*, String>();
            m_cssUrlVector = new Vector<String>;
            
            m_wprofComputationVector = new Vector<WprofComputation*>;

	    m_wprofHTMLTagVector = new Vector<WprofHTMLTag*>();
            
            m_wprofPreloadVector = new Vector<WprofPreload*>;

	    TAG = WprofConstants::ins()->TAG().utf8().data();

	    m_timers = new Vector<int>();
        };
        
        void outputAndClearCSSImageMaps() {
            // First, match!
	    // TODO temp comment out for performance issues
	    // consider another way to get this done
            //matchCSSImages();
            
            clearCSSImageMaps();
        }
        
        void clearCSSImageMaps() {
            m_cssContentMap->clear();
            m_cssUrlVector->clear();
        }
        
        void clearWprofComputations() {
            m_wprofComputationVector->clear();
        }
        
        // ---------------------------------------------------------------
        // Methods for preload request
        void clearWprofPreloads() {
            m_wprofPreloadVector->clear();
        }

        void clearElementStartVector() {
            m_elemStartVector->clear();
            ASSERT(m_elemStartVector->isEmpty());
        }
        
        void clearHOLMaps() {
            m_elemStartVector->clear();
            m_elemTypeMap->clear();
        }
        
        String createFilename(String url) {
            // Skip http:// or https://
	    url.replace("http://", "");
	    url.replace("https://", "");
            // Escape illegal chars as a file name
	    url.replace(":", "_");
	    url.replace("/", "_");

	    return url;
        }
        
        void outputAndClear() {
	    // TODO Init DB
	    //openDatabase();
            
            // Record the timestamp of the 'load' event
            fprintf(stderr, "{\"page\": \"%s\"}\n", m_url.utf8().data());
            fprintf(stderr, "{\"DOMLoad\": %lf}\n", monotonicallyIncreasingTime());

            outputAndClearWprofResource();
            outputAndClearHOLMaps();
            outputAndClearCSSImageMaps();
            outputAndClearWprofComputations();
            outputAndClearWprofPreloads();

	    //Also clear the temp element
	    m_tempWprofHTMLTag = NULL;
			
	    fprintf(stderr, "{\"Complete': \"%s\"}\n", m_url.utf8().data());
        }
        
        void clear() {
            clearWprofResource();
            clearHOLMaps();
            clearCSSImageMaps();
            clearWprofComputations();
            clearWprofPreloads();
        }

        void outputAndClearWprofResource() {
            for (unsigned int i = 0; i < m_wprofResourceVector->size(); ++i) {
                // Output one resource info
                WprofResource* info = (*m_wprofResourceVector)[i];
                
                RefPtr<ResourceLoadTiming> timing = info->resourceLoadTiming();
                
                if (!timing)
                    fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"sentTime\": %lf, \"len\": %ld, \"from\": \"%p\", \"type\": \"%d\", \
                            \"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"connId\": %u, \"connReused\": %d, \"cached\": %d}}\n",
                            info->getId(),
                            info->url().utf8().data(),
                            info->timeDownloadStart(),
                            info->bytes(),
                            (void*)info->fromWprofObject(),
			    info->fromType(),
                            info->mimeType().utf8().data(),
                            info->expectedContentLength(),
                            info->httpStatusCode(),
                            info->connectionId(),
                            info->connectionReused(),
                            info->wasCached()
                            );
                else
                    fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"sentTime\": %lf, \"len\": %ld, \"from\": \"%p\", \"type\": \"%d\", \
                            \"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"connId\": %u, \"connReused\": %d, \"cached\": %d,\
                            \"requestTime\": %f, \"proxyStart\": %d, \"proxyEnd\": %d, \"dnsStart\": %d, \"dnsEnd\": %d, \"connectStart\": %d, \"connectEnd\": %d, \"sendStart\": %d, \"sendEnd\": %d, \"receiveHeadersEnd\": %d, \"sslStart\": %d, \"sslEnd\": %d}}\n",
                            info->getId(),
                            info->url().utf8().data(),
                            info->timeDownloadStart(),
                            info->bytes(),
                            (void*)info->fromWprofObject(),
			    info->fromType(),
                            info->mimeType().utf8().data(),
                            info->expectedContentLength(),
                            info->httpStatusCode(),
                            info->connectionId(),
                            info->connectionReused(),
                            info->wasCached(),
                            timing->requestTime,
                            timing->proxyStart,
                            timing->proxyEnd,
                            timing->dnsStart,
                            timing->dnsEnd,
                            timing->connectStart,
                            timing->connectEnd,
                            timing->sendStart,
                            timing->sendEnd,
                            timing->receiveHeadersEnd,
                            timing->sslStart,
                            timing->sslEnd
                            );
                
                // Output info of received data chunks
                Vector<WprofReceivedChunk*>* v = info->receivedChunkInfoVector();
                for (unsigned int j = 0; j < v->size(); ++j) {
                    WprofReceivedChunk* chunkInfo = (*v)[j];
                    fprintf(stderr, "{\"ReceivedChunk\": {\"receivedTime\": %lf, \"len\": %ld}}\n",
                            chunkInfo->time(),
                            chunkInfo->len()
                    );
                }
	    }
                
	    // Output info of parsed objects
	    //Vector<WprofHTMLTag*>* vWprofHTMLTag = info->derivedWprofHTMLTagVector();
	    for (unsigned int j = 0; j < m_wprofHTMLTagVector->size(); ++j) {
	      WprofHTMLTag* tag = (*m_wprofHTMLTagVector)[j];
	      //Get the urls that might be requested by this tag, note that scripts
	      //can request multiple resources, like images.
	      Vector<String>* urls = tag->urls();
		    
	      fprintf(stderr, "{\"WprofHTMLTag\": {\"code\": \"%p\", \"doc\": \"%s\", \"row\": %d, \"column\": %d, \"tagName\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urls\":  [ ",
		      tag,
		      tag->docUrl().utf8().data(),
		      tag->pos().m_line.zeroBasedInt(),
		      tag->pos().m_column.zeroBasedInt(),
		      tag->tagName().utf8().data(),
		      tag->startTime(),
		      tag->endTime());

	      size_t numUrls = urls->size();
	      size_t i = 0;
	      for(Vector<String>::iterator it = urls->begin(); it!= urls->end(); it++){
		if (i == (numUrls -1)){
		  fprintf(stderr, "\"%s\"", it->utf8().data());
		}
		else{
		  fprintf(stderr, "\"%s\", ", it->utf8().data());
		}
		i++;
	      }

	      fprintf(stderr, " ], \"pos\": %d, \"chunkLen\": %d, \"isStartTag\": %d, \"isFragment\": %d}}\n",
		      tag->startTagEndPos(),
		      tag->chunkLen(),
		      tag->isStartTag(),
		      tag->isFragment()
		      );
	    }
            
            clearWprofResource();
        }
        
        void outputAndClearHOLMaps() {
            // Output start tag vectors
            unsigned int i = 0;
            for (; i < m_elemStartVector->size(); i++) {
                WprofHTMLTag* startObjHash = (*m_elemStartVector)[i];
                
                if (startObjHash == NULL)
                    continue;
                
                // Skip non-script and non-css
                if (!m_elemTypeMap->get(startObjHash))
                    continue;
                
                TextPosition pos_s = startObjHash->m_textPosition;
                fprintf(stderr, "{\"HOL\": {\"type\": %d, \"docUrl\": \"%s\", \"code\": \"%p\", \"row\": %d, \"column\": %d, \"url\": \"%s\"}}\n",
                        m_elemTypeMap->get(startObjHash),
                        startObjHash->m_docUrl.utf8().data(),
                        startObjHash,
                        pos_s.m_line.zeroBasedInt(),
                        pos_s.m_column.zeroBasedInt(),
                        startObjHash->m_url.utf8().data()
                );
            }
            
            // Note that all should be cleared at the same time because
            // we don't hard copy WprofHTMLTag and deletion of one could make
            // another problematic
            clearHOLMaps();
        }
        
        void outputAndClearWprofComputations() {
            for (unsigned int i = 0; i < m_wprofComputationVector->size(); ++i) {
                WprofComputation* event = (*m_wprofComputationVector)[i];
                
				if (event == NULL)
					continue;
					
                if (event->fromWprofHTMLTag() == NULL)
                    continue;
                
                // m_tempWprofHTMLTag->docUrl() indicates current url
                //if (strcmp(event->fromWprofHTMLTag()->docUrl(), m_tempWprofHTMLTag->docUrl()) == 0)
                //	continue;
                
                fprintf(stderr, "{\"Computation\": {\"type\": \"%s\", \"code\": \"%p\", \"from\": \"%p\", \"docUrl\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urlRecalcStyle\": \"%s\"}}\n",
                        event->getTypeForPrint().utf8().data(),
			event,
                        event->fromWprofHTMLTag(),
                        event->fromWprofHTMLTag()->docUrl().utf8().data(),
                        event->startTime(),
                        event->endTime(),
                        event->urlRecalcStyle().utf8().data()
                );
            }
            
            clearWprofComputations();
        }
        
        void outputAndClearWprofPreloads() {
            for (unsigned int i = 0; i < m_wprofPreloadVector->size(); ++i) {
                WprofPreload* pr = (*m_wprofPreloadVector)[i];
                
                if (pr->fromWprofHTMLTag() == NULL){
		  fprintf(stderr, "{\"Preload\": {\"code\": \"\", \"scriptCode\": \"%p\", \"docUrl\": \"%s\", \"url\": \"%s\", \"time\": %lf}}\n",
			pr->executingScriptTag(),
                        pr->executingScriptTag()->docUrl().utf8().data(),
                        pr->url().utf8().data(),
                        pr->time());
		}
		else{
		  fprintf(stderr, "{\"Preload\": {\"code\": \"%p\", \"scriptCode\": \"%p\", \"docUrl\": \"%s\", \"url\": \"%s\", \"time\": %lf}}\n",
			pr->fromWprofHTMLTag(),
			pr->executingScriptTag(),
                        pr->fromWprofHTMLTag()->docUrl().utf8().data(),
                        pr->url().utf8().data(),
			pr->time());
		}
            }
        }
        
        void clearWprofResource() {
            m_wprofResourceVector->clear();
            m_wprofResourceMap->clear();
            m_requestTimeMap->clear();
            m_wprofHTMLTagVector->clear();
        }
        
        /*
         * Get the request time of a url and delete its corresponding mapping
         *
         * @param const char* request url
         * @return double request time
         */
        double getTimeAndRemoveMapping(String url) {

	  double time = -1;
            HashMap<String, double>::iterator iter = m_requestTimeMap->begin();
            
            for (; iter != m_requestTimeMap->end(); ++iter) {
                if (url == iter->first) {
		  time = iter->second;
                }
            }

	    m_requestTimeMap->remove(url);

	    //Check the current state, if we are in the completing phase, then we should output 
            return time;
        }

        /*
         * Append WprofHTMLTag to its corresponding WprofResource.
         *
         * @param WprofHTMLTag*
         */
        void appendDerivedWprofHTMLTag(WprofHTMLTag* tag) {
	  m_wprofHTMLTagVector->append(tag);
	  /*for (unsigned int i = 0; i < m_wprofResourceVector->size(); ++i) {
                WprofResource* resource = (*m_wprofResourceVector)[i];
                if (resource->url() == tag->docUrl()) {
                    resource->appendDerivedWprofHTMLTag(tag);
                    return;
                }
		}*/
        }

        void setTempWprofHTMLTag(WprofHTMLTag* tempWprofHTMLTag) {
            m_tempWprofHTMLTag = tempWprofHTMLTag;

            // We clear this because this is only valid for current HTMLLinkElement
            // TODO to be verified
            m_HTMLLinkRecalcStyle = emptyString();
        }
        
        // --------
        // All info we need for a resource
        // We store by both a vector and a hash with id of url as the key
        Vector<WprofResource*>* m_wprofResourceVector;
        // <id, WprofResource>
        HashMap<unsigned long, WprofResource*>* m_wprofResourceMap;
        // This is ugly but creating WprofResource in ResourceLoader::willSendRequest
        // results in a pointer but. Thus, we create this map in ResourceLoader::willSendRequest
        // and match it with WprofResource later.
        // <url, request time>
        HashMap<String, double>* m_requestTimeMap;
        
	// This is to store the information of a resource request that is made from an WprofHTMLTag
        // to infer dependency. Because this occurs before a request is made, we need
        // to separately store it rather than storing it in WprofResource which is similar
        // to m_requestTimeMap
        //HashMap<String, WprofHTMLTag*>* m_requestWprofHTMLTagMap;
	//HashMap<ResourceRequest, WprofHTMLTag*> * m_requestWprofHTMLTagMap;

	//Mapping between a document and its associated html tag positions (offsets)
	HashMap<Document*, CurrentPosition*>* m_documentCurrentPositionMap;

	//Mapping betwen document fragments and their associated html tag offsets
	HashMap<DocumentFragment*, CurrentPosition*>* m_fragmentCurrentPositionMap;
	
	Vector<WprofHTMLTag*>* m_wprofHTMLTagVector;
	
        // --------
        // Head-of-line dependencies: only CSS and JS are taken into account.
        // CSS blocks following s_exec(JS) until e_parse(CSS)
        // JS:
        // - normal: both download and exec block parsing
        // - defer:  s_exec triggered by dom load
        // - async:  s_exec triggered by e_download
        // See WebCore/html/parser/HTMLTreeBuilder.cpp for more details
        Vector<WprofHTMLTag*>* m_elemStartVector; // s_download(element) > position
        HashMap<WprofHTMLTag*, int>* m_elemTypeMap; // 1: normal; 2: defer; 3: async; 4: CSS
        
        // --------
        // To track how images are fetched from css, we create this hash map so that
        // we can match the urls at this level. We do not pass WprofHTMLTag to each CSSValue
        // or CSSProperty because 1) it is too expensive to do so; 2) it requires us to
        // change a lot of code.
        HashMap<WprofHTMLTag*, String>* m_cssContentMap;
        // They are img urls (from CSSImageValue) for matching.
        // Note that we set two urls (partial and complete) at a time.
        Vector<String>* m_cssUrlVector;
        
        // --------
        // Track computational events when what triggers them
        // This includes "recalcStyle", "layout" and "paint"
        Vector<WprofComputation*>* m_wprofComputationVector;
        
        // --------
        // Track preload_scanner
        Vector<WprofPreload*>* m_wprofPreloadVector;

	//Preloads that have not been matched with an HTML tag that references them
	Vector<WprofPreload*>* m_unmatchedPreloads;

	//A list of timers that have been installed, and we are waiting for them to be fired
	Vector<int>* m_timers;
        
        // --------
        // Temp WprofHTMLTag
        WprofHTMLTag* m_tempWprofHTMLTag;
        int m_charConsumed;
        int m_charLen;
        String m_HTMLLinkRecalcStyle;

	//Store the current load or DOMContentLoaded event
	Event* m_currentEvent;
	
        // DOM counters so as to control when to output info
        int m_domCounter;
        String m_url; // document location, used as file name
	String m_uid;
	const char* TAG;

	enum WprofControllerState {
	  WPROF_BEGIN = 1,
	  WPROF_WAITING_LAST_RESOURCE
	} m_state;
};
    
}
#endif // WPROF_DISABLED

#endif


