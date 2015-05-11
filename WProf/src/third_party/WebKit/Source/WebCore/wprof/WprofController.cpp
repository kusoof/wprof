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

#include "WprofController.h"
#include "WprofElement.h"
#include "WprofGenTag.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Document.h"
#include "DocumentFragment.h"
#include "ResourceRequest.h"

namespace WebCore {


  WprofController::WprofController()
  {
  }
   
  /*
   * Use Singleton to update and fetch Wprof data
   * Note that the notion of using Singleton is problematic if multiple pages
   * are loaded in a single process.
   */
  WprofController* WprofController::getInstance() {
    static WprofController* m_pInstance;
    if (!m_pInstance)
      m_pInstance = new WprofController();
    return m_pInstance;
  };
        
  WprofController::~WprofController() {};

  WprofPage* WprofController::getWprofPage(Page* page)
  {
    if(m_pageMap.contains(page)){
      return m_pageMap.get(page);
    }
    else {
      //complain?
      fprintf(stderr,"We do not have a wprof page for page %p\n", page);
    }
    return NULL;
  }
  
  Page* WprofController::getPageFromDocument(Document* document)
  {
    //Try to get the document's page
    Page* page = document->page();
    if (!page && document->parentDocument() && document->parentDocument()->page()){
      page = document->parentDocument()->page();
    }
    if(!page){
      fprintf(stderr, "the document page is NULL\n");
    }
    return page;
  }
  
  void WprofController::createPage(Page* page){
    WprofPage* wpage = new WprofPage(page);
    m_pageMap.set(page, wpage);
  }

  void WprofController::pageClosed(Page* page){
    WprofPage* wpage = getWprofPage(page);
    wpage->output();
  }


  void WprofController::addDocument(Document* document){
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->addDocument(document);
    }
  }

  WprofGenTag* WprofController::tempElementForPage(Page* page){
    WprofPage* wpage = getWprofPage(page);
    return wpage->tempWprofGenTag();
  }

  WprofComputation* WprofController::getCurrentComputationForPage(Page* page){
    WprofPage* wpage = getWprofPage(page);
    return wpage->getCurrentComputation();
  }
        
  /*
   * This function creates a WprofResource object.
   * Called by ResourceLoader::didReceiveResponse().
   * To save memory, WprofResource merges the <url, request time> mapping.
   *
   * @param
   */
  void WprofController::createWprofResource(unsigned long resourceId,
					    ResourceRequest& request,
					    const ResourceResponse& response,
					    Page* page)
  {
    //Dispatch to the correct page
    WprofPage* wpage = getWprofPage(page);
    if(wpage->isComplete()){
      fprintf(stderr, "the page has already completed, why are we downloading resources?\n");
    }
    wpage->createWprofResource(resourceId, request, response);
  }					   

  void WprofController::createWprofCachedResource(unsigned long resourceId,
						  ResourceRequest& request,
						  Page* page)
  {
    WprofPage* wpage = getWprofPage(page);
    wpage->createWprofCachedResource(resourceId, request);
  }
    
  /*
   * This function creates a WprofReceivedChunk object.
   * Called by ResourceLoader::didReceiveData().
   *
   * @param unsigned long id of the corresponding request
   * @param unsigned long length of the received chunk
   */
  void WprofController::createWprofReceivedChunk(unsigned long id_url, unsigned long length, Page* page) {
    LOG(DependencyLog, "WprofController::addWprofReceivedChunk %lu", id_url);
    WprofPage* wpage = getWprofPage(page);
    wpage->createWprofReceivedChunk(id_url, length);
  }
        
  /*
   * This function adds a <id, request time> mapping.
   * Called by ResourceLoader::willSendRequest().
   *
   * @param resource identifier
   */
  void WprofController::createRequestTimeMapping(unsigned long resourceId, Page* page) {
    WprofPage* wpage = getWprofPage(page);
    wpage->createRequestTimeMapping(resourceId);
  }

  void WprofController::createResourceElementMapping(unsigned long resourceId, WprofElement* element, Page* page) {
    WprofPage* wpage = getWprofPage(page);
    wpage->createResourceElementMapping(resourceId, element);
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
  void WprofController::createWprofHTMLTag(TextPosition textPosition,
					   String docUrl,
					   Document* document,
					   String token,
					   bool isStartTag) {

    //Try to get the document's page
    Page* page = getPageFromDocument(document);

    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->createWprofHTMLTag(textPosition, docUrl, document, token, isStartTag);
    }
    else{
      //Complain!
      fprintf(stderr, "The page for the document is NULL\n");
    }

  }

  void WprofController::createWprofGenTag(String docUrl,
					  Document* document,
					  String token){

    //Try to get the document's page
    Page* page = getPageFromDocument(document);

    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->createWprofGenTag(docUrl, document, token);
    }
    else{
      //Complain!
      fprintf(stderr, "The page for the document is NULL\n");
    }

  }

  //Wprof tags created from parsing a document fragment.
  void WprofController::createWprofHTMLTag(TextPosition textPosition,
					   String docUrl,
					   DocumentFragment* fragment,
					   String token,
					   bool isStartTag) {

    //Try to get the document's page
    Document* document = fragment->document();
    Page* page = getPageFromDocument(document);
    
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->createWprofHTMLTag(textPosition, docUrl, document, token, isStartTag);
    }
    else{
      //Complain!
    }
  }

  void WprofController::setElementTypePair(WprofGenTag* element, int value){
    element->page()->setElementTypePair(element, value);
  }

  /*
   * Create a WprofComputation object.
   * Called by TODO
   *
   * @param int type of the WprofComputation
   */
  WprofComputation* WprofController::createWprofComputation(WprofComputationType type, Page* page) {
    if(!page){
      fprintf(stderr, "The page is null when attempting to create a computation of type %d\n", type);
      return NULL;
    }
    WprofPage* wpage = getWprofPage(page);
    WprofComputation* comp = wpage->createWprofComputation(type);
    return comp;
  }

  WprofComputation* WprofController::createWprofComputation(WprofComputationType type, WprofElement* element){
    if(!element){
      fprintf(stderr, "The element is null when attempting to create a computation of type %d\n", type);
      return NULL;
    }
    WprofPage* wpage = element->page();
    WprofComputation* comp = wpage->createWprofComputation(type, element);
    return comp;
  }

  WprofEvent* WprofController::createWprofEvent(String name, WprofEventTargetType targetType, String info, String docUrl, Page* page) {
    if(!page){
      fprintf(stderr, "The page is null when attempting to create an event\n");
      return NULL;
    }
    WprofPage* wpage = getWprofPage(page);
    WprofEvent* event = wpage->createWprofEvent(name, targetType, NULL, info, docUrl);
    return event;
  }

  WprofEvent* WprofController::createWprofEvent(String name, WprofEventTargetType targetType, WprofElement* target, String info, String docUrl){
    if(!target){
      fprintf(stderr, "The element is null when attempting to create an event\n");
      return NULL;
    }
    WprofPage* wpage = target->page();
    WprofEvent* event = wpage->createWprofEvent(name, targetType, target, info, docUrl);
    return event;
  }

  void WprofController::willFireEventListeners(Event* event, WprofComputation* comp, Page* page)
  {
    //fprintf(stderr, "Will fire event of type %s\n", event->type().string().utf8().data());
    WprofPage* wpage = getWprofPage(page);
    wpage->willFireEventListeners(event, comp);
  }

  void WprofController::didFireEventListeners(Page* page)
  {
    WprofPage* wpage = getWprofPage(page);
    wpage->didFireEventListeners();
  }

  /* ------------------------------- ----------------------- 
     Timers
     ----------------------------------------------------------*/
	
  void WprofController::installTimer(int timerId, int timeout, bool singleShot, Document* d)
  {
    Page* page = getPageFromDocument(d);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->installTimer(timerId, timeout, singleShot);
    }
  }

  void WprofController::removeTimer(int timerId, Document* d)
  {
    Page* page = getPageFromDocument(d);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->removeTimer(timerId);
    }
  }

  WprofComputation*  WprofController::willFireTimer(int timerId, Document* d)
  {
    WprofComputation* comp = NULL;
    Page* page = getPageFromDocument(d);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      comp = wpage->willFireTimer(timerId);
    }
    return comp;
  }

  void WprofController::didFireTimer(int timerId, WprofComputation* comp, Document* d)
  {
    Page* page = getPageFromDocument(d);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->didFireTimer(timerId, comp);
    }
  }

  /*
   * Create a WprofPreload object.
   * Called by HTMLPreloadScanner::preload().
   *
   * @param String the preloaded url
   */
  void WprofController::createWprofPreload(Document* document, ResourceRequest& request, String url, String tagName, int line, int column) {

    String docUrl = document->url().string();
    //String url = request.url().string();
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      request.setWprofPage(wpage);
      wpage->createWprofPreload(url, docUrl, tagName, line, column);
    }
  }
        
  // CSS -> Image doesn't need this because this kind of dependency is
  // inferred by text matching
  void WprofController::createRequestWprofElementMapping(String url, ResourceRequest& request, WprofElement* element) {
    WprofPage* wpage = element->page();
    if(wpage->isComplete()){
      fprintf(stderr, "the page has already completed, why are we setting the request tag mapping from the wrong page?\n");
    }
    wpage->createRequestWprofElementMapping(url, request, element);
  }
        
  void WprofController::createRequestWprofElementMapping(String url, ResourceRequest& request, Page* page) {
    WprofPage* wpage = getWprofPage(page);
    if(wpage->isComplete()){
      fprintf(stderr, "the page has already completed, why are we setting the request tag mapping from the wrong page?\n");
    }
    wpage->createRequestWprofElementMapping(url, request);
  }

  void WprofController::redirectRequest(String url, String redirectUrl, ResourceRequest& request, unsigned long resourceId, Page* page)
  {
    WprofPage* wpage = getWprofPage(page);
    if(wpage->isComplete()){
      fprintf(stderr, "the page has already completed, why are we setting the request tag mapping from the wrong page?\n");
    }
    wpage->redirectRequest(url, redirectUrl, request, resourceId);
  }

  void WprofController::addCharactersConsumed(int numberChars, Document* document, int row){
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->addCharactersConsumed(numberChars, document, row);
    }
  }

  void WprofController::addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row){

    Page* page = getPageFromDocument(fragment->document());
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->addCharactersConsumed(numberChars, fragment, row);
    }
  }

  // ---------------------------------------------------------------
  // Output methods
  // ---------------------------------------------------------------
        
  // Increase/decrease DOM counter, called in Document.cpp
  void WprofController::increaseDomCounter(Document* document) {
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->increaseDomCounter(document);
    }
  }

  void WprofController::decreaseDomCounter(Document* document) {
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->decreaseDomCounter(document);
    }
  }

  void WprofController::setWindowLoadEventFired(Document* document)
  {
    Page* page = getPageFromDocument(document);
    if(page){
      WprofPage* wpage = getWprofPage(page);
      wpage->setWindowLoadEventFired(document);
    }
  }    
}


