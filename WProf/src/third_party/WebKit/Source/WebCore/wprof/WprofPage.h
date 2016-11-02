
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
#include <wtf/RefCounted.h>

//Wprof includes
#include "WprofResource.h"
#include "WprofComputation.h"
//#include "WprofHTMLTag.h"
//#include "WprofElement.h"
//#include "WprofPreload.h"

//WebCore includes
#include "SharedBuffer.h"
/*#include "Page.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "ResourceRequest.h"
#include "Event.h"*/

//Other includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stack>
#include <utility>
#include <string>

#include "Logging.h"
#include "ResourceLoadTiming.h"


namespace WebCore
{
  class ResourceRequest;
  class ResourceResponse;

  class WprofResource;
  class WprofHTMLTag;
  class WprofGenTag;
  class WprofElement;
  class WprofPreload;
  class WprofReceivedChunk;
  class WprofCachedResource;

  class Page;
  class Document;
  class DocumentFragment;
  class Event;
  class Frame;
  class SharedBuffer;

  typedef struct CurrentPosition {
    int position;
    int lastSeenRow;
    CurrentPosition(int p, int l){
      position = p;
      lastSeenRow = l;
    }
  } CurrentPosition;

  typedef struct FrameSourceChange{
    unsigned long frameId;
    String url;
    WprofComputation* comp;

    FrameSourceChange(unsigned long id, String u, WprofComputation* c){
      frameId = id;
      url = u;
      comp = c;
    }
  } FrameSourceChange;
  
  class WprofPage {

  public:
    /*----------------------------------------------------------------
      Constructor/Desctructor
      ---------------------------------------------------------------*/
    WprofPage(Page* page);
    ~WprofPage();

    Page* page ();

    void setOutputPath(const String& outputPath);

    /*----------------------------------------------------------------
      Document Addition
      --------------------------------------------------------------*/
    void addDocument(Document* document);

    /*----------------------------------------------------------------
      Resource Creation/Manipulation
      ---------------------------------------------------------------*/

    
    void createWprofResource(unsigned long resourceId,
			     Frame* frame,
			     ResourceRequest& request,
			     const ResourceResponse& response);

    /*
     * This function creates a WprofReceivedChunk object.
     * Called by ResourceLoader::didReceiveData().
     *
     * @param unsigned long id of the corresponding request
     * @param unsigned long length of the received chunk
     */
    void createWprofReceivedChunk(unsigned long resourceId, const char* data, int length);
    
    /*
     * This function adds a <url, request time> mapping.
     * Called by ResourceLoader::willSendRequest().
     * 
     * Also creates a mapping between the frame pointer and resource identifier if this
     * is the first time we see the frame.
     * 
     * @param const char request url
     */
    void createRequestTimeMapping(unsigned long resourceId, Frame* frame);

    //Needed for redirects
    void createResourceElementMapping(unsigned long resourceId, WprofElement* element);

    /* ------------------------------------------------------------------------
       Cached Resource Records
       ------------------------------------------------------------------------*/
    void createWprofCachedResource(unsigned long resourceId,
				   unsigned size,
				   ResourceRequest& request,
				   const ResourceResponse& response,
				   Frame* frame);


    /*-------------------------------------------------------------------------
      Deal with MessagePort postMessage
      -------------------------------------------------------------------------*/

    void appendWprofComputationForPostMessage();

    WprofComputation* getComputationForRecentPostMessage();

    

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
			    Frame* frame,
			    DocumentFragment* fragment,
			    String token,
			    bool isStartTag);

    //Wprof element created from javascript using createElement
    void createWprofGenTag(String docUrl,
			   Document* document,
			   String token);


    // Place the html tag inside the request for later reference when we actually receive the response.
    void createRequestWprofElementMapping(String url, ResourceRequest& request, WprofElement* element);
    void createRequestWprofElementMapping(String url, ResourceRequest& request);
    void redirectRequest(String url, String redirectUrl, ResourceRequest& request, unsigned long resourceId);

    /* ------------------------------------------------------------------------
       Computation Creation
       ----------------------------------------------------------------------- */
    /*
     * Create a WprofComputation object.
     *
     * @param int type of the WprofComputation
     */
    WprofComputation* createWprofComputation(WprofComputationType type);
    WprofComputation* createWprofComputation(WprofComputationType type, WprofElement* element);

    void setCurrentComputationComplete();

    WprofComputation* getCurrentComputation();

    WprofEvent* createWprofEvent(String name, WprofEventTargetType targetType, WprofElement* target, String info, String docUrl, Frame* frame);

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
	
    void installTimer(int timerId, int timeout, bool singleShot);

    void removeTimer(int timerId);

    WprofComputation*  willFireTimer(int timerId);

    void didFireTimer(int timerId, WprofComputation* comp);

    WprofGenTag* tempWprofGenTag();

    void willFireEventListeners(Event* event, WprofComputation* comp);
    void didFireEventListeners();

    //Output the page contents to a file
    void output();

    bool isComplete() {
      return m_complete;
    }

    void setElementTypePair(WprofGenTag* element, int value);

    void addWprofFrameSourceChange(Frame* frame, String url, WprofComputation* comp);
    void setFrameLoadTime(Frame* frame);

    String pageURL();

  private:
    /*------------------------------------------------------------------------------
      Helper Methods
      ---------------------------------------------------------------------------*/

    //Match a preloaded resource with the HTML tag that references it
    void matchWithPreload(WprofGenTag* tag, String tagUrl);

    // For temporarily stored obj hash
    void setTempWprofGenTag(WprofGenTag* tempWprofGenTag);

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
        
    void clearHOLMaps();

    void clearWprofResources();

    void clearWprofCachedResources();
        
    String createFilename(String url);
        
    void clear();

    void outputWprofResources();

    void outputWprofCachedResources();
        
    void outputHOLMaps();
        
    void outputWprofComputations();
        
    void outputWprofPreloads();

    //Methods for creating directories
    void CheckAndCreateDirectory(const String& path);

    String CreateDirectoryRecursive(const String& url, const String& outputPath);
        
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

    // A map from a frame identifier to the WprofFrame object (which contains info about frame resource, parent, and load time)
    HashMap<unsigned long, WprofFrame*> m_frameMap;

    // A list of changes to a particular frame's source url, and the computation that triggered it
    Vector<FrameSourceChange*> m_frameSrcChanges;
    
    // This is ugly but creating WprofResource in ResourceLoader::willSendRequest
    // results in a pointer but. Thus, we create this map in ResourceLoader::willSendRequest
    // and match it with WprofResource later.
    // <url, request time>
    HashMap<unsigned long, double> m_requestTimeMap;

    //A map between a resource identifier and the element that referenced the resource
    HashMap<unsigned long, WprofElement*> m_identifierElementMap;

    // A vector for the HTML tags for the documents of this page.
    Vector<WprofGenTag*> m_tags;

    // --------
    // Track computational events and what triggers them
    // This includes "recalcStyle", "layout" and "paint"
    Vector<WprofComputation*> m_computations;

    // -------
    // A list of the cached resource accesses 
    Vector<WprofCachedResource*> m_cachedResources;
        
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
    HashMap<WprofGenTag*, int> m_elementTypeMap; // 1: normal; 2: defer; 3: async; 4: CSS
    
    //A map of timers that have been installed, and waiting to fire, pointing to the computations that triggered them
    HashMap<int, WprofComputation*> m_timers;

    //A map of the timer installed and its timeout value
    HashMap<int, int> m_timeouts;

    //A queue of computations that have called MessagePort postMessage
    Vector<WprofComputation*> m_postMessageComputations;
        
    // --------
    // Temp WprofElement, which could be an HTML tag, or a javascript generated element
    WprofGenTag* m_tempWprofGenTag;
    int m_charConsumed;

    //Store the current computation or load or DOMContentLoaded events in a stack
    Event* m_currentEvent;
    std::stack<WprofComputation*> m_computationStack;
	
    // DOM counters so as to control when to output info
    int m_domCounter;

    bool m_complete;

    String m_outputPath;

    enum WprofControllerState {
      WPROF_BEGIN = 1,
      WPROF_WAITING_LAST_RESOURCE
    } m_state;
  };
}

#endif
