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
#include "wtf/ExportMacros.h"
#include "Logging.h"
#include "ResourceLoadTiming.h"
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
    static WprofController* getInstance();
        
    ~WprofController();
        
    /*
     * This function creates a WprofResource object.
     * Called by ResourceLoader::didReceiveResponse().
     * To save memory, WprofResource merges the <url, request time> mapping.
     *
     * @param
     */
    void createWprofResource(unsigned long id,
			     ResourceRequest& request,
			     RefPtr<ResourceLoadTiming> resourceLoadTiming,
			     String mime,
			     long long expectedContentLength,
			     int httpStatusCode,
			     unsigned connectionId,
			     bool connectionReused,
			     bool wasCached);

    /*
     * This function creates a WprofReceivedChunk object.
     * Called by ResourceLoader::didReceiveData().
     *
     * @param unsigned long id of the corresponding request
     * @param unsigned long length of the received chunk
     */
    void createWprofReceivedChunk(unsigned long id_url, unsigned long length);
        
    /*
     * This function adds a <url, request time> mapping.
     * Called by ResourceLoader::willSendRequest().
     * To save memory, especially that of the url, we should move this info to WprofResource
     * a corresponding WprofResource object is created.
     *
     * @param const char request url
     */
    void createRequestTimeMapping(String url);
  

    void matchWithPreload(WprofHTMLTag* tag);

    /*
     * This function creates a WprofHTMLTag object.
     * Called by HTMLTreeBuilder::constructTreeFromToken().
     *
     * @param TextPosition textPosition of the tag
     * @param String url of the document
     * @param String token of the tag
     * @param bool whether this is a start tag
     */
    void createWprofHTMLTag(TextPosition textPosition,
			    String docUrl,
			    Document* document,
			    String token,
			    bool isStartTag);

    //Wprof tags created from parsing a document fragment.
    void createWprofHTMLTag(TextPosition textPosition,
			    String docUrl,
			    DocumentFragment* fragment,
			    String token,
			    bool isStartTag);

    /*
     * Create a WprofComputation object.
     * Called by TODO
     *
     * @param int type of the WprofComputation
     */
    WprofComputation* createWprofComputation(int type);

    WprofComputation* createWprofComputation(int type, WprofHTMLTag* tag);

    void willFireEventListeners(Event* event);

    void didFireEventListeners();


    /* ------------------------------- ----------------------- 
       Timers
       ----------------------------------------------------------*/
	
    void installTimer(int timerId, int timeout, bool singleShot);

    void removeTimer(int timerId);

    WprofComputation*  willFireTimer(int timerId);

    void didFireTimer(int timerId, WprofComputation* comp);

    /*
     * Create a WprofPreload object.
     * Called by HTMLPreloadScanner::preload().
     *
     * @param String the preloaded url
     */
    void createWprofPreload(String url, String docUrl, String tagName, int line, int column);
        
    // CSS -> Image doesn't need this because this kind of dependency is
    // inferred by text matching
    void createRequestWprofHTMLTagMapping(ResourceRequest& request, WprofHTMLTag* tag);
        
    void createRequestWprofHTMLTagMapping(ResourceRequest& request);

    void addElementStart(WprofHTMLTag* tag);
        
    void setElementTypePair(WprofHTMLTag* key, int value);
        
    // For temporarily stored obj hash
    WprofHTMLTag* tempWprofHTMLTag();
        
    String HTMLLinkRecalcStyle();
    void setHTMLLinkRecalcStyle(String url);
        
    int charConsumed();
    int charLen();
    void setCharConsumed(int charConsumed, int charLen);

    void addCharactersConsumed(int numberChars, Document* document, int row);

    void addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row);
        
    // ---------------------------------------------------------------
    // Methods for CSS elements (images could be requested from CSS)
        
    void addCSSUrl(String url);
        
    void clearCSSUrlVector();
        
    void setCSSContentPair(WprofHTMLTag* key, String value);
        
    // ---------------------------------------------------------------
    // Methods for computational events
    WprofComputation* getWprofComputation();
    // ---------------------------------------------------------------
    // Output methods
    // ---------------------------------------------------------------
        
    // Increase/decrease DOM counter, called in Document.cpp
    void increaseDomCounter();
    void setWindowLoadEventFired(String url);

    bool hasPageLoaded ();
        
    void decreaseDomCounter(String url);

    void setPageLoadComplete();

    bool setDocumentURL(String url);
        
  private:
  WprofController();
        
  void outputAndClearCSSImageMaps();
        
  void clearCSSImageMaps();
        
  void clearWprofComputations();
        
    // ---------------------------------------------------------------
    // Methods for preload request
  void clearWprofPreloads();

  void clearElementStartVector();
        
  void clearHOLMaps();
        
  String createFilename(String url);
        
  void outputAndClear();
        
  void clear();

  void outputAndClearWprofResource();
        
  void outputAndClearHOLMaps();
        
  void outputAndClearWprofComputations();
        
  void outputAndClearWprofPreloads();
        
  void clearWprofResource();
        
    /*
     * Get the request time of a url and delete its corresponding mapping
     *
     * @param const char* request url
     * @return double request time
     */
  double getTimeAndRemoveMapping(String url);

    /*
     * Append WprofHTMLTag to its corresponding WprofResource.
     *
     * @param WprofHTMLTag*
     */
  void appendDerivedWprofHTMLTag(WprofHTMLTag* tag);

  void setTempWprofHTMLTag(WprofHTMLTag* tempWprofHTMLTag);
        
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


