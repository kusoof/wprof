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
#include "config.h"
#include "Logging.h"
#include "ResourceLoadTiming.h"
#include "WprofComputation.h"
#include "WprofHTMLTag.h"
#include "WprofPage.h"
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
#include "Document.h"
#include "DocumentFragment.h"
#include "ResourceRequest.h"
#include "Page.h"

namespace WebCore {

  class ResourceResponse;
    
  class WprofController {
  public:
  
  
    /*
     * Use Singleton to update and fetch Wprof data
     * Note that the notion of using Singleton is problematic if multiple pages
     * are loaded in a single process.
     */
    static WprofController* getInstance();
      

    /*
      Destructor
    */
    ~WprofController();

    void createPage(Page* page);
    void pageClosed(Page* page);
    void addDocument(Document* document);
        
    void createWprofResource(unsigned long resourceId,
			     ResourceRequest& request,
			     const ResourceResponse& response,
			     Page* page);
    /*
     * This function creates a WprofReceivedChunk object.
     * Called by ResourceLoader::didReceiveData().
     *
     * @param unsigned long id of the corresponding request
     * @param unsigned long length of the received chunk
     */
    void createWprofReceivedChunk(unsigned long id_url, unsigned long length, Page* page);
        
    /*
     * This function adds a <url, request time> mapping.
     * Called by ResourceLoader::willSendRequest().
     * To save memory, especially that of the url, we should move this info to WprofResource
     * a corresponding WprofResource object is created.
     *
     * @param const char request url
     */
    void createRequestTimeMapping(unsigned long resourceId, Page* page);
  
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
     *
     * @param int type of the WprofComputation
     */
    WprofComputation* createWprofComputation(int type, Page* page);
    WprofComputation* createWprofComputation(int type, WprofHTMLTag* tag);

    WprofHTMLTag* tempTagForPage(Page* page);

    void setTagTypePair(WprofHTMLTag* tag, int value);

    /*
      ----------------------------------------------------------
      Event listeners
      ---------------------------------------------------------
    */

    void willFireEventListeners(Event* event, WprofComputation* comp, Page* page);
    void didFireEventListeners(Page* page);


    /* ------------------------------- ----------------------- 
       Timers
       ----------------------------------------------------------*/
	
    /*void installTimer(int timerId, int timeout, bool singleShot);

    void removeTimer(int timerId);

    WprofComputation*  willFireTimer(int timerId);

    void didFireTimer(int timerId, WprofComputation* comp);*/

    /*
     * Create a WprofPreload object.
     * Called by HTMLPreloadScanner::preload().
     *
     * @param String the preloaded url
     */
    void createWprofPreload(Document* document, ResourceRequest& request, String tagName, int line, int column);
        
    // CSS -> Image doesn't need this because this kind of dependency is
    // inferred by text matching
    void createRequestWprofHTMLTagMapping(String url, ResourceRequest& request, WprofHTMLTag* tag);
        
    void createRequestWprofHTMLTagMapping(String url, ResourceRequest& request, Page* page);

    void addCharactersConsumed(int numberChars, Document* document, int row);

    void addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row);
        
    
    // ---------------------------------------------------------------
    // Output methods
    // ---------------------------------------------------------------
        
    // Increase/decrease DOM counter, called in Document.cpp
    void increaseDomCounter(Document* document);
    void setWindowLoadEventFired(Document* document);
        
    void decreaseDomCounter(Document* document);

    void setPageLoadComplete();
    Page* getPageFromDocument(Document* document);

    void setElementTypePair(WprofHTMLTag* tag, int value);
        
  private:
  WprofController();
        
  HashMap<Page*, WprofPage*> m_pageMap;  
  WprofPage* getWprofPage(Page* page);
  };
}
#endif // WPROF_DISABLED

#endif


