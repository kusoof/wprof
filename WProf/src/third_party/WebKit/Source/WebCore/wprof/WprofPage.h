
#ifndef WprofPage_h
#define WprofPage_h

//config file
#include "config.h"

//WTF data structure includes
#include <wtf/Vector.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/CurrentTime.h>
#include <wtf/MD5.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

//Wprof includes
#include "WprofResource.h"
#include "WprofComputation.h"
#include "WprofHTMLTag.h"
#include "WprofPreload.h"
#include "WprofReceivedChunk.h"

//WebCore includes
#include "Page.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "ResourceRequest.h"
#include "Event.h"

//Other includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "Logging.h"
#include "ResourceLoadTiming.h"


namespace WebCore
{
  class ResourceRequest;
  class ResourceResponse;

  typedef struct CurrentPosition {
    int position;
    int lastSeenRow;
    CurrentPosition(int p, int l){
      position = p;
      lastSeenRow = l;
    }
  } CurrentPosition;
  
  class WprofPage {

  public:
    /*----------------------------------------------------------------
      Constructor/Desctructor
      ---------------------------------------------------------------*/
    WprofPage(Page* page);
    ~WprofPage();

    Page* page () {return m_page;}

    /*----------------------------------------------------------------
      Document Addition
      --------------------------------------------------------------*/
    void addDocument(Document* document);

    /*----------------------------------------------------------------
      Resource Creation/Manipulation
      ---------------------------------------------------------------*/

    
    void createWprofResource(unsigned long resourceId,
			     ResourceRequest& request,
			     const ResourceResponse& response);

    /*
     * This function creates a WprofReceivedChunk object.
     * Called by ResourceLoader::didReceiveData().
     *
     * @param unsigned long id of the corresponding request
     * @param unsigned long length of the received chunk
     */
    void createWprofReceivedChunk(unsigned long resourceId, unsigned long length);
        
    /*
     * This function adds a <url, request time> mapping.
     * Called by ResourceLoader::willSendRequest().
     * To save memory, especially that of the url, we should move this info to WprofResource
     * a corresponding WprofResource object is created.
     *
     * @param const char request url
     */
    void createRequestTimeMapping(unsigned long resourceId);

    /*-------------------------------------------------------------------------
      HTML Tag Creation
      --------------------------------------------------------------------------*/

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

    // Place the html tag inside the request for later reference when we actually receive the response.
    void createRequestWprofHTMLTagMapping(String url, ResourceRequest& request, WprofHTMLTag* tag);
    void createRequestWprofHTMLTagMapping(String url, ResourceRequest& request);

    /* ------------------------------------------------------------------------
       Computation Creation
       ----------------------------------------------------------------------- */
    /*
     * Create a WprofComputation object.
     *
     * @param int type of the WprofComputation
     */
    WprofComputation* createWprofComputation(int type);
    WprofComputation* createWprofComputation(int type, WprofHTMLTag* tag);

    /*-------------------------------------------------------------------------
      Preloads
      -------------------------------------------------------------------------*/

    /*
     * Create a WprofPreload object.
     * Called by HTMLPreloadScanner::preload().
     *
     * @param String the preloaded url
     */
    void createWprofPreload(String url, String docUrl, String tagName, int line, int column);

    /*-----------------------------------------------------------------------------
      Characters Parsed
      ----------------------------------------------------------------------------*/

    int charConsumed();
    void addCharactersConsumed(int numberChars, Document* document, int row);
    void addCharactersConsumed(int numberChars, DocumentFragment* fragment, int row);

    /* ----------------------------------------------------------------------------
       Page Completion Events
       ----------------------------------------------------------------------------*/
    // Increase/decrease DOM counter, called in Document.cpp
    void increaseDomCounter(Document* document);
    void decreaseDomCounter(Document* document);
    void setWindowLoadEventFired(Document* document);

    /* ------------------------------- ----------------------- 
       Timers
       ----------------------------------------------------------*/
	
    /*void installTimer(int timerId, int timeout, bool singleShot);

    void removeTimer(int timerId);

    WprofComputation*  willFireTimer(int timerId);

    void didFireTimer(int timerId, WprofComputation* comp);*/

    WprofHTMLTag* tempWprofHTMLTag();

    void willFireEventListeners(Event* event, WprofComputation* comp);
    void didFireEventListeners();

    //Output the page contents to a file
    void output();

    void setTagTypePair(WprofHTMLTag* tag, int value);

  private:
    /*------------------------------------------------------------------------------
      Helper Methods
      ---------------------------------------------------------------------------*/
    void addStartTag(WprofHTMLTag* tag);

    //Match a preloaded resource with the HTML tag that references it
    void matchWithPreload(WprofHTMLTag* tag);

    // For temporarily stored obj hash
    void setTempWprofHTMLTag(WprofHTMLTag* tempWprofHTMLTag);

    /*
     * Get the request time of a url and delete its corresponding mapping
     *
     * @param unsigned long request id
     * @return double request time
     */
    double getTimeForResource(unsigned long requestId);

    WprofComputation* getWprofComputation();
        
    bool hasPageLoaded ();

    void setPageLoadComplete();

    void setPageURL(String url);
        
    // ---------------------------------------------------------------
    // Output methods
    // ---------------------------------------------------------------
            
    void clearWprofComputations();
        
    void clearWprofPreloads();

    void clearStartTags();
        
    void clearHOLMaps();

    void clearWprofResources();
        
    String createFilename(String url);
        
    void clear();

    void outputWprofResources();
        
    void outputHOLMaps();
        
    void outputWprofComputations();
        
    void outputWprofPreloads();
        
    // -------- Instance Variables --------------------

    // The actual WebCore page the WprofPage references
    Page* m_page;

    // A list of the documents contained in the page
    HashSet<Document*> m_documents;

    String m_url; // document location, used as file name
    String m_uid; // Page uid constructed from the url and the time.    
    
    // A vector for the resources downloaded for this page
    Vector<WprofResource*> m_resources;
    
    // A map of resource id -> WprofResource
    HashMap<unsigned long, WprofResource*> m_resourceMap;
    
    // This is ugly but creating WprofResource in ResourceLoader::willSendRequest
    // results in a pointer but. Thus, we create this map in ResourceLoader::willSendRequest
    // and match it with WprofResource later.
    // <url, request time>
    HashMap<unsigned long, double> m_requestTimeMap;

    // A vector for the HTML tags for the documents of this page.
    Vector<WprofHTMLTag*> m_tags;

    // --------
    // Track computational events and what triggers them
    // This includes "recalcStyle", "layout" and "paint"
    Vector<WprofComputation*> m_computations;
        
    // --------
    // Track the preloads
    Vector<WprofPreload*> m_preloads;

    //Preloads that have not been matched with an HTML tag that references them
    Vector<WprofPreload*> m_unmatchedPreloads;

    //Mapping between a document and its associated html tag positions (offsets)
    HashMap<Document*, CurrentPosition*> m_documentCurrentPositionMap;

    //Mapping betwen document fragments and their associated html tag offsets
    HashMap<DocumentFragment*, CurrentPosition*> m_fragmentCurrentPositionMap;
		
    // --------
    // Head-of-line dependencies: only CSS and JS are taken into account.
    // CSS blocks following s_exec(JS) until e_parse(CSS)
    // JS:
    // - normal: both download and exec block parsing
    // - defer:  s_exec triggered by dom load
    // - async:  s_exec triggered by e_download
    // See WebCore/html/parser/HTMLTreeBuilder.cpp for more details
    Vector<WprofHTMLTag*> m_startTags; // s_download(element) > position
    HashMap<WprofHTMLTag*, int> m_tagTypeMap; // 1: normal; 2: defer; 3: async; 4: CSS
    
    //A list of timers that have been installed, and we are waiting for them to be fired
    Vector<int> m_timers;
        
    // --------
    // Temp WprofHTMLTag
    WprofHTMLTag* m_tempWprofHTMLTag;
    int m_charConsumed;

    //Store the current load or DOMContentLoaded event
    Event* m_currentEvent;
    WprofComputation* m_eventComputation;
	
    // DOM counters so as to control when to output info
    int m_domCounter;

    enum WprofControllerState {
      WPROF_BEGIN = 1,
      WPROF_WAITING_LAST_RESOURCE
    } m_state;
  };
}

#endif
