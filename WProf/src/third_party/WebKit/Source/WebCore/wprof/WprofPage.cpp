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

#include "WprofResource.h"
#include "WprofComputation.h"
#include "WprofHTMLTag.h"
#include "WprofGenTag.h"
#include "WprofElement.h"
#include "WprofPreload.h"
#include "WprofCachedResource.h"

#include <Frame.h>

#include "Document.h"
#include "DocumentFragment.h"
#include "Page.h"
#include "Frame.h"
#include "ResourceRequest.h"

namespace WebCore {


  WprofPage::WprofPage(Page* page)
    : m_page(page)
    , m_tempWprofGenTag(NULL)
    , m_charConsumed(0)
    , m_domCounter(0)
    , m_state (WPROF_BEGIN)
  {
    m_currentEvent = 0;
    m_complete = false;
  }
        
  WprofPage::~WprofPage() {
    clear();
  }

  Page* WprofPage::page () {return m_page;}


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
				      Frame* frame,
				      ResourceRequest& request,
				      const ResourceResponse& response){

    RefPtr<ResourceLoadTiming> resourceLoadTiming = response.resourceLoadTiming();
    String mime = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    int httpStatusCode = response.httpStatusCode();
    unsigned connectionId = response.connectionID();
    bool connectionReused = response.connectionReused();
    bool wasCached = response.wasCached();
    String httpMethod = request.httpMethod();

    //Get the URL from the request
    String url(request.url().string());
	    
    // Find request start time
    double time = getTimeForResource(resourceId);

    /*if(request.wprofElement()){
	WprofElement* tag = request.wprofElement();
	Vector<WprofElement*>::iterator it = m_tags.begin();
	bool found = false;
	for(; it != m_tags.end(); it++){
	  if ((*it) == tag){
	    found = true;
	    break;
	  }
	}
	
	if(!found){
	  fprintf(stderr, "The tag is not found in the page\n");
	}
	}*/
            
    // Find the WprofHTMLTag that this resource is requested from if any
    // [Note] deep copy request url
    WprofResource* resource = new WprofResource(resourceId,
						url,
						frame,
						resourceLoadTiming,
						mime,
						expectedContentLength,
						httpStatusCode,
						httpMethod,
						connectionId,
						connectionReused,
						wasCached,
						time,
						request.wprofElement());
            
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
    else 
    { 
      fprintf(stderr, "received chunk resource identifier not found %ld\n", resourceId);
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
  void WprofPage::createRequestTimeMapping(unsigned long resourceId, Frame* frame) {
    m_requestTimeMap.set(resourceId, monotonicallyIncreasingTime());

    //If this is the first time we encounter the frame, we are in fact downloading the frame resource
    //Add a mapping to the identifier
    if (!m_frameMap.contains(frame->identifier())){
      unsigned long parentId = 0;
      if (frame->tree()->parent()){
	parentId = frame->tree()->parent()->identifier();
      }
      WprofFrame* wframe = new WprofFrame(frame->identifier(), parentId, resourceId);
      m_frameMap.set(frame->identifier(), wframe);
    }
  }

  void WprofPage::createResourceElementMapping(unsigned long resourceId, WprofElement* element){
    if(m_identifierElementMap.get(resourceId)){
      
    }
    m_identifierElementMap.set(resourceId, element);
  }

  /* ------------------------------------------------------------------------
       Cached Resource Records
       ------------------------------------------------------------------------*/
  void WprofPage::createWprofCachedResource(unsigned long resourceId,
					    unsigned size,
					    ResourceRequest& request,
					    const ResourceResponse& response,
					    Frame* frame)
  {
    //Get the URL from the request
    String url(request.url().string());
	    
    // Find the current time for the cache access
    double time = monotonicallyIncreasingTime();


    String mime = response.mimeType();
    String httpMethod = request.httpMethod();

    unsigned long frameId = 0;
    if(frame){
      frameId = frame->identifier();
    }
      
    WprofCachedResource* cached = new WprofCachedResource(resourceId,
							  url,
							  time,
							  mime,
							  size,
							  httpMethod,
							  frameId,
							  request.wprofElement());
							  
    m_cachedResources.append(cached);
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
					 document->frame(),
					 textPosition,
					 docUrl,
					 token,
					 charPos->position,				     
					 isFragment,
					 isStartTag);


    // Add WprofHTMLTag to its vector
    m_tags.append(tag);

    setTempWprofGenTag(tag);

    if (isStartTag) {
      
      if (token == String::format("script")) {
	// Add tag to tempWprofHTMLTag for EndTag
	// This works because there is no children inside elements
	// we care about (<script>, <link> and <style>)

	// Set type as normal if we already hadn't set it. We will change it if async or defer
	if (!m_elementTypeMap.contains(tag)){
	  setElementTypePair(tag, 1); // normal
	}
      }
    }
    //Set computation if it triggered the current tag
    if(!m_computationStack.empty()){
      WprofComputation* currentComputation = m_computationStack.top();
      tag->setParentComputation(currentComputation);
    }

    if (isFragment && !tag->parentComputation()){
      fprintf(stderr, "in gen tag for doc, fragment and no computation!\n");
      //The best we can do is attribute the fragment to the previous computation
      //since the computation didn't trigger a pumptokenizer to get through in HTMLDocumentParser
      WprofComputation* previousComputation = m_computations.last();
      if (!previousComputation->isRenderType()){
	tag->setParentComputation(previousComputation);
      }
    }
    //tag->print();
  }

  void WprofPage::createWprofGenTag(String docUrl,
				    Document* document,
				    String token){

    

    WprofGenTag* element = new WprofGenTag(this, document->frame(), docUrl, token);
    // Add WprofHTMLTag to its vector
    m_tags.append(element);

    setTempWprofGenTag(element);
    
    if (token == String::format("script")) {
      // Add tag to tempWprofHTMLTag for EndTag
      // This works because there is no children inside elements
      // we care about (<script>, <link> and <style>)

      // Set type as normal. We will change it if async or defer
      if(!m_elementTypeMap.contains(element)){
	setElementTypePair(element, 1); // normal
      }
    }
    
    //Set computation if it triggered the event
    if(!m_computationStack.empty()){
      WprofComputation* currentComputation = m_computationStack.top();
      element->setParentComputation(currentComputation);
    }
    else{
      fprintf(stderr, "computation is null for element %s\n", token.utf8().data());
    }
    //element->print();
  }

  //Wprof tags created from parsing a document fragment.
  void WprofPage::createWprofHTMLTag(TextPosition textPosition,
				     String docUrl,
				     Frame* frame,
				     DocumentFragment* fragment,
				     String token,
				     bool isStartTag) {

    //Get the entry corresponding to the fragment
    CurrentPosition* charPos = m_fragmentCurrentPositionMap.get(fragment);

    WprofHTMLTag* tag = new WprofHTMLTag(this,
					 frame,
					 textPosition,
					 docUrl,
					 token,
					 charPos->position,
					 true,
					 isStartTag);


    // Add WprofHTMLTag to its vector
    m_tags.append(tag);

    setTempWprofGenTag(tag);

    if (isStartTag) {
      
      if (token == String::format("script")) {
	// Add tag to tempWprofHTMLTag for EndTag
	// This works because there is no children inside elements
	// we care about (<script>, <link> and <style>)

	// Set type as normal. We will change it if async or defer
	if (!m_elementTypeMap.contains(tag)){
	  setElementTypePair(tag, 1); // normal
	}
      }
    }

    //If we are in the middle of executing an event, then the parsed tag will depend on the computation
    //Set computation if it triggered the parsing
    if(!m_computationStack.empty()){
      WprofComputation* currentComputation = m_computationStack.top();
      tag->setParentComputation(currentComputation);
    }

    if (!tag->parentComputation()){
      fprintf(stderr, "in gen tag for fragment, fragment and no computation!\n");
      //The best we can do is attribute the fragment to the previous computation
      //since the computation didn't trigger a pumptokenizer to get through in HTMLDocumentParser
      WprofComputation* previousComputation = m_computations.last();
      if(! previousComputation->isRenderType()){
	tag->setParentComputation(previousComputation);
      }
    }
    //tag->print();
  }

  
  // Attach the html tag to the request so we can refer to it later when the resource has finished downloading.
  void WprofPage::createRequestWprofElementMapping(String url, ResourceRequest& request, WprofElement* element) {
    
    request.setWprofPage(this);
    
    //Check to see if we have this tag
    if(element && !element->isComputation()){
      Vector<WprofGenTag*>::iterator it = m_tags.begin();
      bool found = false;
      for(; it != m_tags.end(); it++){
	if ((*it) == element){
	  found = true;
	  break;
	}
      }

      if(!found){
	fprintf(stderr, "The tag is not found in the page\n");
      }
    }

    WprofElement* resourceParent = element;

    if(!element || (!element->isComputation() && (!((WprofGenTag*)element)->name().contains(String::format("iframe"))))){
      //Check whether we have a current computation
      WprofComputation* currentComputation = NULL;
      if(!m_computationStack.empty()){
	currentComputation = m_computationStack.top();
      }

      //Note: iframes can be downloaded after we encounter their tag, i.e. they are scheduled for later
      //In this case, the computation didn't really trigger the iframe download
      if(currentComputation && (!element || (element->parent() != currentComputation))){
      
	//If the selected tag has the computation as the predecessor,
	//then it's ok to let the resource's predecessor be the tag, not the computation

	//Otherwise use the computation as parent
	resourceParent = currentComputation;
      }
    }

    //This could be the very first request for the page, which has a null tag.
    if(resourceParent){
      //Set the request's element correctly (either tag or computation)
      request.setWprofElement(resourceParent);
      //Add the url to the list
      resourceParent->appendUrl(url);

       //Attempt to match the preloaded resource to the request from this tag.
      if(!resourceParent->isComputation()){
	matchWithPreload((WprofGenTag*)resourceParent, url);
      }
    }
  }

  //If this is a redirect, remove the original url from the tag, and add the new url
  void WprofPage::redirectRequest(String url, String redirectUrl, ResourceRequest& request, unsigned long resourceId) {

    //Try to find the tag associated with the resource
    WprofElement* element = NULL;
    if(m_identifierElementMap.contains(resourceId)){
      element = m_identifierElementMap.get(resourceId);
    }
    
    if(element){
      element->removeUrl(redirectUrl);
      
      if(element){
	request.setWprofElement(element);
	  
	//Add the url to the list
	element->appendUrl(url);
      
	//No attempt to match with a preload. TODO: does this break things?	
      } 
    }
    request.setWprofPage(this);
  }
        
  //If we have no tag, then the best we can do is match the request to the most recent tag.
  void WprofPage::createRequestWprofElementMapping(String url, ResourceRequest& request) {
    createRequestWprofElementMapping(url, request, tempWprofGenTag());
  }


  /*-------------------------------------------------------------------
    Computations
    ------------------------------------------------------------------*/

  /*
   * Create a WprofComputation object.
   *
   * @param int type of the WprofComputation
   */
  WprofComputation* WprofPage::createWprofComputation(WprofComputationType type) {
    return createWprofComputation(type, NULL);
  }

  WprofComputation* WprofPage::createWprofComputation(WprofComputationType type, WprofElement* element){

    if(!element){
      element = tempWprofGenTag();
    }

    WprofComputation* event = new WprofComputation(type, element, this);
    m_computations.append(event);

    //if this is a script, fire event, css, or timer, then set the current event
    if(!event->isRenderType()){
      m_computationStack.push(event);
      //fprintf(stderr, "the start computation is here %s\n", event->getTypeForPrint().utf8().data()); 
    }

    return event;
  }

  void WprofPage::setCurrentComputationComplete(){
    if(!m_computationStack.empty()){
      WprofComputation* event = m_computationStack.top();
      m_computationStack.pop();
      //fprintf(stderr, "the end computation is here %s\n", event->getTypeForPrint().utf8().data()); 
    }
  }

  WprofComputation* WprofPage::getCurrentComputation(){
    WprofComputation* comp = NULL;
    if(!m_computationStack.empty()){
      comp = m_computationStack.top();
    }
    return comp;
  }

  WprofEvent* WprofPage::createWprofEvent(String name, WprofEventTargetType targetType, WprofElement* target, String info, String docUrl, Frame* frame)
  {
    WprofEvent* event = new WprofEvent(name, target, targetType, info, docUrl, frame, this);
    m_computations.append(event);
    m_computationStack.push(event);

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
    WprofPreload* preload = new WprofPreload(tempWprofGenTag(), url, docUrl, tagName, line, column);
    m_preloads.append(preload);
    m_unmatchedPreloads.append(preload);
  }

  void WprofPage::matchWithPreload(WprofGenTag* tag, String tagUrl){
    Vector<WprofPreload*>::iterator it = m_unmatchedPreloads.begin();
    size_t position = 0;
    for(; it != m_unmatchedPreloads.end(); it++){
      if(tag->matchesPreload((*it), tagUrl)){
	//Only a WprofHTMLTag can match
	(*it)->setFromTag((WprofHTMLTag*) tag);
	m_unmatchedPreloads.remove(position);
	break;
      }
      position++;
    }
  }

  /*----------------------------------------------------------------------
    MessagePort post message
    ----------------------------------------------------------------------*/

  void WprofPage::appendWprofComputationForPostMessage(){
    WprofComputation* comp = getCurrentComputation();
    if(comp){
      m_postMessageComputations.append(comp);
    }
  }

  WprofComputation* WprofPage::getComputationForRecentPostMessage(){
    if (m_postMessageComputations.size()){
      WprofComputation* first = m_postMessageComputations[0];
      m_postMessageComputations.remove(0);
      return first;
    }
    return NULL;
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
    //m_computationStack.push(comp);
  }

  void WprofPage::didFireEventListeners()
  {
    m_currentEvent = NULL;
    /*if(!m_computationStack.empty()){
      m_computationStack.pop();
      }*/
  }


  /* ------------------------------- ----------------------- 
     Timers
     ----------------------------------------------------------*/
	
  void WprofPage::installTimer(int timerId, int timeout, bool singleShot)
  {
    if(!m_computationStack.empty()){
      WprofComputation* currentComputation = m_computationStack.top();
      m_timers.set(timerId, currentComputation);
    }
    else{
      fprintf(stderr, "no computation for timer with id %d\n", timerId);
    }
    
    m_timeouts.set(timerId, timeout);
  }

  void WprofPage::removeTimer(int timerId)
  {
    /*if(m_timers.contains(timerId)){
      m_timers.remove(timerId);
      m_timeouts.remove(timerId);
      }*/
  }

  WprofComputation*  WprofPage::willFireTimer(int timerId)
  {
    //Get the computation that triggered the timer
    WprofComputation* parent = NULL;
    if(m_timers.contains(timerId)){
      parent = m_timers.get(timerId);
    }
    else{
      fprintf(stderr, "we don't have a computation for timer with id %d\n", timerId);
    }
    
    WprofComputation* comp = createWprofComputation(ComputationTimer, parent);
    
    if(m_timeouts.contains(timerId)){
      int timeout = m_timeouts.get(timerId);
      comp->setUrlRecalcStyle(String::format("%d", timeout));
    }
    else{
      fprintf(stderr, "we don't have a timeout for timer with id %d\n", timerId);
    }
    return comp;
  }

  void WprofPage::didFireTimer(int timerId, WprofComputation* comp)
  {
    comp->end();
    removeTimer(timerId);
  }
        
  void WprofPage::setElementTypePair(WprofGenTag* key, int value) {
    if (key == NULL)
      return;

    m_elementTypeMap.set(key, value);
  }

  void WprofPage::addWprofFrameSourceChange(Frame* frame, String url, WprofComputation* comp)
  {
    FrameSourceChange* change= new FrameSourceChange(frame->identifier(), url, comp);
    m_frameSrcChanges.append(change);
  }

  void WprofPage::setFrameLoadTime(Frame* frame){
    unsigned long frameId = frame->identifier();
    if (m_frameMap.contains(frameId)){
      WprofFrame* wframe = m_frameMap.get(frameId);
      wframe->setLoadTime(monotonicallyIncreasingTime());
    }
  }
        
  // For temporarily stored obj hash
  WprofGenTag* WprofPage::tempWprofGenTag() { return m_tempWprofGenTag; }
        
        
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
    //Copy the url string
    String urlCopy(url);
    // Now we should have doc that only begins with http
    StringBuilder stringBuilder;
    m_url = urlCopy;
    //m_url = createFilename(urlCopy);
    m_uid = m_url + String::number(monotonicallyIncreasingTime());
  }
  
  String WprofPage::pageURL(){
    return m_url;
  }
  
  /* ----------------------------------------------------------------
     Clearing Methods
  ------------------------------------------------------------------*/
          
  void WprofPage::clearWprofComputations() {
    fprintf(stderr, "clearing the computations\n");
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
        
  void WprofPage::clearHOLMaps() {
    m_elementTypeMap.clear();
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
    outputWprofCachedResources();
    outputHOLMaps();
    outputWprofComputations();
    outputWprofPreloads();
    
			
    fprintf(stderr, "{\"Complete\": \"%s\"}\n", m_url.utf8().data());
    m_complete = true;
  }
        
  void WprofPage::clear() {
    clearWprofResources();
    clearWprofCachedResources();
    clearHOLMaps();
    clearWprofComputations();
    clearWprofPreloads();
  }

  /*-----------------------------------------------------------------------------------
    Output Methods
    -----------------------------------------------------------------------------------*/

  void WprofPage::outputWprofResources() {

    //First output the frame mapping
    for (HashMap<unsigned long, WprofFrame*>::iterator it = m_frameMap.begin(); it != m_frameMap.end(); ++it){
      it->second->print();
    }
    
    //And then the frame source changes
    for (Vector<FrameSourceChange*>::iterator it = m_frameSrcChanges.begin(); it != m_frameSrcChanges.end(); it++){
      fprintf(stderr, "{\"FrameChange\": {\"code\": \"%ld\", \"url\": \"%s\", \"comp\": \"%p\"}}\n", (*it)->frameId, (*it)->url.utf8().data(), (*it)->comp);
    }
    
    for (unsigned int i = 0; i < m_resources.size(); ++i) {
      // Output one resource info
      WprofResource* info = m_resources[i];
                
      RefPtr<ResourceLoadTiming> timing = info->resourceLoadTiming();
                
      if (!timing)
	fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"frame\": \"%ld\", \"sentTime\": %lf, \"recieveTime\": %lf, \"len\": %ld, \"from\": \"%p\", \
\"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"httpMethod\": \"%s\", \"connId\": %u, \"connReused\": %d, \"cached\": %d}}\n",
		info->getId(),
		info->url().utf8().data(),
		info->frameId(),
		info->timeDownloadStart(),
		info->timeReceiveComplete(),
		info->bytes(),
		(void*)info->fromWprofObject(),
		info->mimeType().utf8().data(),
		info->expectedContentLength(),
		info->httpStatusCode(),
		info->httpMethod().utf8().data(),
		info->connectionId(),
		info->connectionReused(),
		info->wasCached()
		);
      else
	fprintf(stderr, "{\"Resource\": {\"id\": %ld, \"url\": \"%s\", \"frame\": \"%ld\", \"sentTime\": %lf,  \"recieveTime\": %lf, \"len\": %ld, \"from\": \"%p\", \
\"mimeType\": \"%s\", \"contentLength\": %lld, \"httpStatus\": %d, \"httpMethod\": \"%s\", \"connId\": %u, \"connReused\": %d, \"cached\": %d, \
                            \"requestTime\": %f, \"proxyStart\": %d, \"proxyEnd\": %d, \"dnsStart\": %d, \"dnsEnd\": %d, \"connectStart\": %d,\
                            \"connectEnd\": %d, \"sendStart\": %d, \"sendEnd\": %d, \"receiveHeadersEnd\": %d, \"sslStart\": %d, \"sslEnd\": %d}}\n",
		info->getId(),
		info->url().utf8().data(),
		info->frameId(),
		info->timeDownloadStart(),
		info->timeReceiveComplete(),
		info->bytes(),
		(void*)info->fromWprofObject(),
		info->mimeType().utf8().data(),
		info->expectedContentLength(),
		info->httpStatusCode(),
		info->httpMethod().utf8().data(),
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
	fprintf(stderr, "{\"ReceivedChunk\": {\"resourceId\": %ld, \"receivedTime\": %lf, \"len\": %ld}}\n",
		info->getId(),
		chunkInfo->time(),
		chunkInfo->len()
		);
      }
    }
                
    // Output info of parsed objects
    for (unsigned int j = 0; j < m_tags.size(); ++j) {
      WprofElement* element = m_tags[j];
      element->print();
    }
  }

  void WprofPage::outputWprofCachedResources()
  {
    for(unsigned int i= 0; i < m_cachedResources.size(); i++){
      WprofCachedResource* cached = m_cachedResources[i];
      cached->print();
    }
  }
        
  void WprofPage::outputHOLMaps() {

    // Output the blocking scripts map
    for(HashMap<WprofGenTag*, int>::iterator it = m_elementTypeMap.begin(); it != m_elementTypeMap.end(); ++it){
      //lookup the actual wprof tag
      WprofGenTag* tag = it->first;
      fprintf(stderr, "{\"HOL\": {\"type\": %d, \"docUrl\": \"%s\", \"code\": \"%p\"}}\n",
	      it->second,
	      tag->docUrl().utf8().data(),
	      tag);
    }
  }
        
  void WprofPage::outputWprofComputations() {
    for (unsigned int i = 0; i < m_computations.size(); ++i) {
      WprofComputation* event = m_computations[i];
                
      /*if (event == NULL)
      	continue;*/
					
      if ((event->fromWprofElement() == NULL) && (event->type() != ComputationFireEvent)){
	continue;
      }
                
      event->print();
    }
  }
        
  void WprofPage::outputWprofPreloads() {
    for (unsigned int i = 0; i < m_preloads.size(); ++i) {
      WprofPreload* pr = m_preloads[i];
                
      if (pr->fromWprofHTMLTag() == NULL){
	fprintf(stderr, "{\"Preload\": {\"code\": \"(nil)\", \"scriptCode\": \"%p\", \"docUrl\": \"%s\", \"url\": \"%s\", \"tag\": \"%s\", \"row\": %d, \"column\": %d, \"time\": %lf}}\n",
		pr->executingScriptTag(),
		pr->executingScriptTag()->docUrl().utf8().data(),
		pr->url().utf8().data(),
		pr->tagName().utf8().data(),
		pr->line(),
		pr->column(),
		pr->time());
      }
      else{
	fprintf(stderr, "{\"Preload\": {\"code\": \"%p\", \"scriptCode\": \"%p\", \"docUrl\": \"%s\", \"url\": \"%s\", \"tag\": \"%s\", \"row\": %d, \"column\": %d, \"time\": %lf}}\n",
		pr->fromWprofHTMLTag(),
		pr->executingScriptTag(),
		pr->fromWprofHTMLTag()->docUrl().utf8().data(),
		pr->url().utf8().data(),
		pr->tagName().utf8().data(),
		pr->line(),
		pr->column(),
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

  void WprofPage::clearWprofCachedResources(){
    m_cachedResources.clear();
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

  void WprofPage::setTempWprofGenTag(WprofGenTag* tempWprofGenTag) {
    m_tempWprofGenTag = tempWprofGenTag;

  }
    
}


