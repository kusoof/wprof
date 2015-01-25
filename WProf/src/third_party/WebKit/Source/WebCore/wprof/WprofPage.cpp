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

#include "WprofPage.h"
#include <Frame.h>

namespace WebCore {


  WprofPage::WprofPage(Page* page)
    : m_page(page)
    , m_tempWprofHTMLTag(NULL)
    , m_charConsumed(0)
    , m_domCounter(0)
    , m_state (WPROF_BEGIN)
  {
    m_currentEvent = 0;
    m_eventComputation = 0;
  }
        
  WprofPage::~WprofPage() {
    clear();
  }


  /*--------------------------------------------------------------------------
    Document Additions
    --------------------------------------------------------------------------*/

  void WprofPage::addDocument(Document* document){
    //Add the document to our list
    m_documents.add(document);
    //If the document's frame is the page's main frame, then set the URL
    if(document->frame()){
      Frame* frame = document->frame();
      if(frame == (frame->page()->mainFrame())){
	setPageURL(document->url().string());
      }
    }
  }
  /*--------------------------------------------------------------------------------
    Resource Creation/Manipulation
    -------------------------------------------------------------------------------*/

  /*
   * This function creates a WprofResource object.
   * Called by ResourceLoader::didReceiveResponse().
   * To save memory, WprofResource merges the <url, request time> mapping.
   *
   * @param
   */
  void WprofPage::createWprofResource(unsigned long resourceId,
				      ResourceRequest& request,
				      const ResourceResponse& response){

    RefPtr<ResourceLoadTiming> resourceLoadTiming = response.resourceLoadTiming();
    String mime = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    int httpStatusCode = response.httpStatusCode();
    unsigned connectionId = response.connectionID();
    bool connectionReused = response.connectionReused();
    bool wasCached = response.wasCached();

    //Get the URL from the request
    String url(request.url().string());
	    
    // Find request start time
    double time = getTimeForResource(resourceId);
            
    // Find the WprofHTMLTag that this resource is requested from if any
    // [Note] deep copy request url
    WprofResource* resource = new WprofResource(resourceId,
						url,
						resourceLoadTiming,
						mime,
						expectedContentLength,
						httpStatusCode,
						connectionId,
						connectionReused,
						wasCached,
						time,
						request.wprofHTMLTag());
            
    // Add to both Vector and HashMap
    m_resources.append(resource);
    m_resourceMap.set(resource->getId(), resource);

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
  void WprofPage::createWprofReceivedChunk(unsigned long resourceId, unsigned long length) {
    WprofReceivedChunk* chunk = new WprofReceivedChunk(resourceId, length, monotonicallyIncreasingTime());
    WprofResource* resource = m_resourceMap.get(resourceId);
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
  void WprofPage::createRequestTimeMapping(unsigned long resourceId) {
    m_requestTimeMap.set(resourceId, monotonicallyIncreasingTime());
  }

  /*---------------------------------------------------------------------------------
    HTML Tags
    -------------------------------------------------------------------------------*/
  /*
   * This function creates a WprofHTMLTag object.
   * Called by HTMLTreeBuilder::constructTreeFromToken().
   *
   * @param TextPosition textPosition of the tag
   * @param String url of the document
   * @param String token of the tag
   * @param bool whether this is a start tag
   */
  void WprofPage::createWprofHTMLTag(TextPosition textPosition,
				     String docUrl,
				     Document* document,
				     String token,
				     bool isStartTag) {

    //Get the entry corresponding to the doc url
    CurrentPosition* charPos = m_documentCurrentPositionMap.get(document);

    bool isFragment = false;

    Document* ownerDocument = document->parentDocument();

    if(ownerDocument && (ownerDocument->url().string() == docUrl)){
      //If the URLs of the owner and document are the same, then we have a fragment
      isFragment = true;
    }

    WprofHTMLTag* tag = new WprofHTMLTag(this,
					 textPosition,
					 docUrl,
					 token,
					 charPos->position,				     
					 isFragment,
					 isStartTag);

    // Add WprofHTMLTag to its vector
    m_tags.append(tag);

    setTempWprofHTMLTag(tag);

    if (isStartTag) {
      // Add tag to the start vector
      addStartTag(tag);

      if (token == String::format("script")) {
	// Add tag to tempWprofHTMLTag for EndTag
	// This works because there is no children inside elements
	// we care about (<script>, <link> and <style>)

	// Set type as normal. We will change it if async or defer
	setTagTypePair(tag, 1); // normal
      }
    }
    //Set computation if it triggered the event
    if(m_eventComputation){
      tag->setParentComputation(m_eventComputation);
    }
  }

  //Wprof tags created from parsing a document fragment.
  void WprofPage::createWprofHTMLTag(TextPosition textPosition,
				     String docUrl,
				     DocumentFragment* fragment,
				     String token,
				     bool isStartTag) {

    //Get the entry corresponding to the fragment
    CurrentPosition* charPos = m_fragmentCurrentPositionMap.get(fragment);

    WprofHTMLTag* tag = new WprofHTMLTag(this,
					 textPosition,
					 docUrl,
					 token,
					 charPos->position,
					 true,
					 isStartTag);

    // Add WprofHTMLTag to its vector
    m_tags.append(tag);

    setTempWprofHTMLTag(tag);

    if (isStartTag) {
      // Add tag to the start vector
      addStartTag(tag);

      if (token == String::format("script")) {
	// Add tag to tempWprofHTMLTag for EndTag
	// This works because there is no children inside elements
	// we care about (<script>, <link> and <style>)

	// Set type as normal. We will change it if async or defer
	setTagTypePair(tag, 1); // normal
      }
    }

    //If we are in the middle of executing an event, then the parsed tag will depend on the computation
    //Set computation if it triggered the parsing
    if(m_eventComputation){
      tag->setParentComputation(m_eventComputation);
    }
  }

  
  // Attach the html tag to the request so we can refer to it later when the resource has finished downloading.
  void WprofPage::createRequestWprofHTMLTagMapping(String url, ResourceRequest& request, WprofHTMLTag* tag) {	  	  
    
    //Assign the tag to the request
    //This could be the very first request for the page, which has a null tag.
    if(tag){
      request.setWprofHTMLTag(tag);
	  
      //Add the url to the list
      tag->appendUrl(url);
      
      //Attempt to match the preloaded resource to the request from this tag.
      matchWithPreload(tag);
    }
    request.setWprofPage(this);
  }
        
  //If we have no tag, then the best we can do is match the request to the most recent tag.
  void WprofPage::createRequestWprofHTMLTagMapping(String url, ResourceRequest& request) {
    createRequestWprofHTMLTagMapping(url, request, tempWprofHTMLTag());
  }


  /*-------------------------------------------------------------------
    Computations
    ------------------------------------------------------------------*/

  /*
   * Create a WprofComputation object.
   *
   * @param int type of the WprofComputation
   */
  WprofComputation* WprofPage::createWprofComputation(int type) {
    WprofComputation* event = new WprofComputation(type, tempWprofHTMLTag());
    m_computations.append(event);

    return event;
  }

  WprofComputation* WprofPage::createWprofComputation(int type, WprofHTMLTag* tag){

    if(!tag){
      tag = tempWprofHTMLTag();
    }

    WprofComputation* event = new WprofComputation(type, tag);
    m_computations.append(event);

    return event;
  }

  /*-----------------------------------------------------------------
    Preloads
    ---------------------------------------------------------------------*/

  /*
   * Create a WprofPreload object.
   * Called by HTMLPreloadScanner::preload().
   *
   * @param String the preloaded url
   */
  void WprofPage::createWprofPreload(String url, String docUrl, String tagName, int line, int column) {
    WprofPreload* preload = new WprofPreload(tempWprofHTMLTag(), url, docUrl, tagName, line, column);
    m_preloads.append(preload);
    m_unmatchedPreloads.append(preload);
  }

  void WprofPage::matchWithPreload(WprofHTMLTag* tag){
    Vector<WprofPreload*>::iterator it = m_unmatchedPreloads.begin();
    size_t position = 0;
    TextPosition textPosition = tag->pos();
    for(; it != m_unmatchedPreloads.end(); it++){
      if((*it)->matchesToken(tag->docUrl(), tag->urls(), tag->tagName(), textPosition.m_line.zeroBasedInt(), textPosition.m_column.zeroBasedInt())){
	(*it)->setFromTag(tag);
	m_unmatchedPreloads.remove(position);
	break;
      }
      position++;
    }
  }

  /*------------------------------------------------------------------------
    Parse Characters Consumed
    ---------------------------------------------------------------------*/
  int WprofPage::charConsumed() { 
    return m_charConsumed; 
  }

  void WprofPage::addCharactersConsumed(int numberChars, Document* document, int row){
    //First get the entry in the map, corresponding to the document
    if(!m_documentCurrentPositionMap.contains(document)){
      CurrentPosition* charPos = new CurrentPosition(numberChars, row);
      m_documentCurrentPositionMap.set(document, charPos);
    }
    else{
      CurrentPosition* charPos = m_documentCurrentPositionMap.get(document);
      if(charPos->lastSeenRow <= row){
	charPos->position += numberChars;
	charPos->lastSeenRow = row;
      }
    }
    m_charConsumed+= numberChars;
  }

  void WprofPage::addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row){
    //First get the entry in the map, corresponding to the fragment
    if(!m_fragmentCurrentPositionMap.contains(fragment)){
      //fprintf(stderr, "adding new document with pointer %p\n", document);
      CurrentPosition* charPos = new CurrentPosition(numberChars, row);
      m_fragmentCurrentPositionMap.set(fragment, charPos);
    }
    else{
      CurrentPosition* charPos = m_fragmentCurrentPositionMap.get(fragment);
      if(charPos->lastSeenRow <= row){
	charPos->position += numberChars;
	charPos->lastSeenRow = row;
      }
    }
    m_charConsumed+= numberChars;
  }

  /* -----------------------------------------------------------------------------
     Event Listeners
     ------------------------------------------------------------------------------*/
  void WprofPage::willFireEventListeners(Event* event, WprofComputation* comp)
  {
    //fprintf(stderr, "Will fire event of type %s\n", event->type().string().utf8().data());
    m_currentEvent = event;
    m_eventComputation = comp;
  }

  void WprofPage::didFireEventListeners()
  {
    m_currentEvent = NULL;
    m_eventComputation = NULL;
  }


  /* ------------------------------- ----------------------- 
     Timers
     ----------------------------------------------------------*/
	
  /*void WprofPage::installTimer(int timerId, int timeout, bool singleShot)
  {
    m_timers.append(timerId);
    //fprintf(stderr, "adding timer with id %d\n", timerId);
  }

  void WprofPage::removeTimer(int timerId)
  {
    for(size_t i = 0; i != m_timers.size(); i++){
      if(m_timers[i] == timerId){
	m_timers.remove(i);
	break;
      }
    }

    //Check if we can complete the page load
    if(hasPageLoaded()){
      setPageLoadComplete();
    }
  }

  WprofComputation*  WprofPage::willFireTimer(int timerId)
  {
    WprofComputation* comp = createWprofComputation(5);
    comp->setUrlRecalcStyle("Timer");
    return comp;
  }

  void WprofPage::didFireTimer(int timerId, WprofComputation* comp)
  {
    comp->end();
    //fprintf(stderr, "timer did fire with id %d\n", timerId);
    removeTimer(timerId);
    }*/
        
  void WprofPage::addStartTag(WprofHTMLTag* tag) {
    m_startTags.append(tag);
  }
        
  void WprofPage::setTagTypePair(WprofHTMLTag* key, int value) {
    if (key == NULL)
      return;
            
    // Should check whether key has existed
    HashMap<WprofHTMLTag*, int>::iterator iter = m_tagTypeMap.begin();
    for (; iter != m_tagTypeMap.end(); ++iter) {
      if (*key == *iter->first) {
	m_tagTypeMap.set(iter->first, value);
	return;
      }
    }
    m_tagTypeMap.set(key, value);
  }
        
  // For temporarily stored obj hash
  WprofHTMLTag* WprofPage::tempWprofHTMLTag() { return m_tempWprofHTMLTag; }
        
        
  // ---------------------------------------------------------------
  // Methods for computational events
  WprofComputation* WprofPage::getWprofComputation() {
    return m_computations.last();
  }

  // ---------------------------------------------------------------
  // Output methods
  // ---------------------------------------------------------------
        
  // Increase/decrease DOM counter, called in Document.cpp
  void WprofPage::increaseDomCounter(Document* document) { m_domCounter++; }

  void WprofPage::setWindowLoadEventFired(Document* document)
  {
    String url = document->url().string();
    if (m_state == WPROF_BEGIN){
      m_state = WPROF_WAITING_LAST_RESOURCE;
    }

    //if the pending request list is already empty, call the complete event
    if(hasPageLoaded()){
      setPageLoadComplete();
    }
 
  }

  bool WprofPage::hasPageLoaded (){
    return (m_state == WPROF_WAITING_LAST_RESOURCE) && (m_requestTimeMap.isEmpty()) 
      && (m_timers.isEmpty()) && (m_domCounter <=0);
  }
        
  void WprofPage::decreaseDomCounter(Document* document) {
    m_domCounter--;

    if(hasPageLoaded()){
      setPageLoadComplete();
    }
  }

  void WprofPage::setPageLoadComplete() {
    m_state = WPROF_BEGIN;
  }

  void WprofPage::setPageURL(String url)
  {             
    fprintf(stderr, "before set page url is %s\n", url.utf8().data()); 
    //Copy the url string
    String urlCopy(url);
    // Now we should have doc that only begins with http
    StringBuilder stringBuilder;

    m_url = createFilename(urlCopy);
    m_uid = m_url + String::number(monotonicallyIncreasingTime());
    fprintf(stderr, "after set page url is %s\n", url.utf8().data()); 
  }
  
  /* ----------------------------------------------------------------
     Clearing Methods
  ------------------------------------------------------------------*/
          
  void WprofPage::clearWprofComputations() {
    for(size_t i = 0; i < m_computations.size(); i++){
      delete m_computations[i];
    }
    m_computations.clear();
  }
        
  void WprofPage::clearWprofPreloads() {
    for(size_t i = 0; i < m_preloads.size(); i++){
      delete m_preloads[i];
    }
    m_preloads.clear();
  }

  void WprofPage::clearStartTags() {
    for(size_t i = 0; i < m_startTags.size(); i++){
      delete m_startTags[i];
    }
    m_startTags.clear();
    ASSERT(m_startTags.isEmpty());
  }
        
  void WprofPage::clearHOLMaps() {
    clearStartTags();
    m_tagTypeMap.clear();
  }
        
  String WprofPage::createFilename(String url) {
    // Skip http:// or https://
    url.replace("http://", "");
    url.replace("https://", "");
    // Escape illegal chars as a file name
    url.replace(":", "_");
    url.replace("/", "_");

    return url;
  }
        
  void WprofPage::output() {
    // TODO Init DB
    //openDatabase();
            
    // Record the timestamp of the 'load' event
    fprintf(stderr, "{\"page\": \"%s\"}\n", m_url.utf8().data());
    fprintf(stderr, "{\"DOMLoad\": %lf}\n", monotonicallyIncreasingTime());

    outputWprofResources();
    outputHOLMaps();
    outputWprofComputations();
    outputWprofPreloads();

    //Also clear the temp element
    m_tempWprofHTMLTag = NULL;
			
    fprintf(stderr, "{\"Complete': \"%s\"}\n", m_url.utf8().data());
  }
        
  void WprofPage::clear() {
    clearWprofResources();
    clearHOLMaps();
    clearWprofComputations();
    clearWprofPreloads();
  }

  /*-----------------------------------------------------------------------------------
    Output Methods
    -----------------------------------------------------------------------------------*/

  void WprofPage::outputWprofResources() {
    for (unsigned int i = 0; i < m_resources.size(); ++i) {
      // Output one resource info
      WprofResource* info = m_resources[i];
                
      RefPtr<ResourceLoadTiming> timing = info->resourceLoadTiming();
                
      if (!timing)
	fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"sentTime\": %lf, \"len\": %ld, \"from\": \"%p\", \
                            \"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"connId\": %u, \"connReused\": %d, \"cached\": %d}}\n",
		info->getId(),
		info->url().utf8().data(),
		info->timeDownloadStart(),
		info->bytes(),
		(void*)info->fromWprofObject(),
		info->mimeType().utf8().data(),
		info->expectedContentLength(),
		info->httpStatusCode(),
		info->connectionId(),
		info->connectionReused(),
		info->wasCached()
		);
      else
	fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"sentTime\": %lf, \"len\": %ld, \"from\": \"%p\", \
                            \"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"connId\": %u, \"connReused\": %d, \"cached\": %d,\
                            \"requestTime\": %f, \"proxyStart\": %d, \"proxyEnd\": %d, \"dnsStart\": %d, \"dnsEnd\": %d, \"connectStart\": %d,\
                            \"connectEnd\": %d, \"sendStart\": %d, \"sendEnd\": %d, \"receiveHeadersEnd\": %d, \"sslStart\": %d, \"sslEnd\": %d}}\n",
		info->getId(),
		info->url().utf8().data(),
		info->timeDownloadStart(),
		info->bytes(),
		(void*)info->fromWprofObject(),
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
    for (unsigned int j = 0; j < m_tags.size(); ++j) {
      WprofHTMLTag* tag = m_tags[j];
      //Get the urls that might be requested by this tag, note that scripts
      //can request multiple resources, like images.
      Vector<String>* urls = tag->urls();
		    
      fprintf(stderr, "{\"WprofHTMLTag\": {\"code\": \"%p\", \"comp\": \"%p\", \"doc\": \"%s\", \"row\": %d, \"column\": %d, \"tagName\": \"%s\",\
                        \"startTime\": %lf, \"endTime\": %lf, \"urls\":  [ ",
	      tag,
	      tag->parentComputation(),
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

      fprintf(stderr, " ], \"pos\": %d, \"isStartTag\": %d, \"isFragment\": %d}}\n",
	      tag->startTagEndPos(),
	      tag->isStartTag(),
	      tag->isFragment()
	      );
    }
  }
        
  void WprofPage::outputHOLMaps() {
    // Output start tag vectors
    unsigned int i = 0;
    for (; i < m_startTags.size(); i++) {
      WprofHTMLTag* startObjHash = m_startTags[i];
                
      if (startObjHash == NULL)
	continue;
                
      // Skip non-script and non-css
      if (!m_tagTypeMap.get(startObjHash))
	continue;
                
      TextPosition pos_s = startObjHash->m_textPosition;
      fprintf(stderr, "{\"HOL\": {\"type\": %d, \"docUrl\": \"%s\", \"code\": \"%p\", \"row\": %d, \"column\": %d, \"url\": \"%s\"}}\n",
	      m_tagTypeMap.get(startObjHash),
	      startObjHash->m_docUrl.utf8().data(),
	      startObjHash,
	      pos_s.m_line.zeroBasedInt(),
	      pos_s.m_column.zeroBasedInt(),
	      startObjHash->m_url.utf8().data()
	      );
    }
  }
        
  void WprofPage::outputWprofComputations() {
    for (unsigned int i = 0; i < m_computations.size(); ++i) {
      WprofComputation* event = m_computations[i];
                
      if (event == NULL)
	continue;
					
      if (event->fromWprofHTMLTag() == NULL)
	continue;
                
      // m_tempWprofHTMLTag->docUrl() indicates current url
      //if (strcmp(event->fromWprofHTMLTag()->docUrl(), m_tempWprofHTMLTag->docUrl()) == 0)
      //	continue;
                
      fprintf(stderr, "{\"Computation\": {\"type\": \"%s\", \"code\": \"%p\", \"from\": \"%p\",\
\"docUrl\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urlRecalcStyle\": \"%s\"}}\n",
	      event->getTypeForPrint().utf8().data(),
	      event,
	      event->fromWprofHTMLTag(),
	      event->fromWprofHTMLTag()->docUrl().utf8().data(),
	      event->startTime(),
	      event->endTime(),
	      event->urlRecalcStyle().utf8().data()
	      );
    }
  }
        
  void WprofPage::outputWprofPreloads() {
    for (unsigned int i = 0; i < m_preloads.size(); ++i) {
      WprofPreload* pr = m_preloads[i];
                
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
        
  void WprofPage::clearWprofResources() {
    m_resources.clear();
    m_resourceMap.clear();
    m_requestTimeMap.clear();
    m_tags.clear();
  }
        
  /*
   * Get the request time of a url and delete its corresponding mapping
   *
   * @param const char* request url
   * @return double request time
   */
  double WprofPage::getTimeForResource(unsigned long resourceId) {

    double time = -1;

    if(m_requestTimeMap.contains(resourceId)){
      time = m_requestTimeMap.get(resourceId);
    }

    return time;
  }

  void WprofPage::setTempWprofHTMLTag(WprofHTMLTag* tempWprofHTMLTag) {
    m_tempWprofHTMLTag = tempWprofHTMLTag;

  }
    
}


