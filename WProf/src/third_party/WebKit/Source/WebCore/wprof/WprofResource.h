/*
 * WprofResource.h
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

#ifndef WprofResource_h
#define WprofResource_h

#if !WPROF_DISABLED

#include "WprofHTMLTag.h"
#include "WprofReceivedChunk.h"

namespace WebCore {

// Define WprofResource
// This is the data structure to store all info about resources
// Note: Created only in ResourceLoader::didReceiveResponse()
//
// This can be retrieved by either a Vector or a HashMap with m_id as the key
class WprofResource {
public:
	/*PassRefPtr<WprofResource> create(
	    unsigned long id,
            const char* url,
            RefPtr<ResourceLoadTiming> resourceLoadTiming,
            const char* mime,
            long long expectedContentLength,
            int httpStatusCode,
            unsigned connectionId,
            bool connectionReused,
            bool wasCached,
            double time,
            WprofHTMLTag* tag) {

	    return resource = adoptRef(new WprofResource(id, url, resourceLoadTiming, mime, expectedContentLength, httpStatusCode, connectionId, connectionReused, wasCached, time, tag));
	}

	void ref() { refWprofResource(); }
        void deref() { derefWprofResource(); }*/

        WprofResource(
	    unsigned long id,
	    String url,
            RefPtr<ResourceLoadTiming> resourceLoadTiming,
            String mime,
            long long expectedContentLength,
            int httpStatusCode,
            unsigned connectionId,
            bool connectionReused,
            bool wasCached,
	    double time,
	    WprofHTMLTag* tag
	)
		: m_bytes(0)
        {
            m_id = id;

	    // [Note] deep copy
            if (resourceLoadTiming != NULL)
            	m_resourceLoadTiming = resourceLoadTiming->deepCopy();

	    m_url = url;
	    m_mimeType = mime;

            m_expectedContentLength = expectedContentLength;
            m_httpStatusCode = httpStatusCode;
            m_connectionId = connectionId;
            m_connectionReused = connectionReused;
            m_wasCached = wasCached;
            m_timeDownloadStart = time;
            m_fromWprofHTMLTag = tag;

	    // Create WprofReceivedChunk and WprofHTMLTag vectors
            m_receivedChunkInfoVector = new Vector<WprofReceivedChunk*>;
            m_derivedWprofHTMLTagVector = new Vector<WprofHTMLTag*>;
        };
        
        ~WprofResource() {};
        
        unsigned long getId() { return m_id; }
        String url() { return m_url; }
        RefPtr<ResourceLoadTiming> resourceLoadTiming() { return m_resourceLoadTiming; }
        String mimeType() { return m_mimeType; }
        long long expectedContentLength() { return m_expectedContentLength; }
        int httpStatusCode() { return m_httpStatusCode; }
        unsigned connectionId() { return m_connectionId; }
        bool connectionReused() { return m_connectionReused; }
        bool wasCached() { return m_wasCached; }
        double timeDownloadStart() { return m_timeDownloadStart; }
        WprofHTMLTag* fromWprofHTMLTag() { return m_fromWprofHTMLTag; }
        unsigned long bytes() { return m_bytes; }

        Vector<WprofReceivedChunk*>* receivedChunkInfoVector() { return m_receivedChunkInfoVector; }
        Vector<WprofHTMLTag*>* derivedWprofHTMLTagVector() { return m_derivedWprofHTMLTagVector; }
        
        // Called only in WprofController::createWprofReceivedChunk()
        void addBytes(unsigned long bytes) {
            m_bytes += bytes;
        }
        
        // Called only in WprofController::createWprofReceivedChunk()
        void appendWprofReceivedChunk(WprofReceivedChunk* info) {
            if (info == NULL)
                return;
            
            // TODO temp comment out
            //ASSERT(info->getId() == getId());
            m_receivedChunkInfoVector->append(info);
        }
        
        void appendDerivedWprofHTMLTag(WprofHTMLTag* tag) {
            ASSERT(url() == tag->docUrl());
            m_derivedWprofHTMLTagVector->append(tag);
        }
        
private:

        unsigned long m_id;
        String m_url;
        double m_timeDownloadStart;
        
        Vector<WprofReceivedChunk*>* m_receivedChunkInfoVector;
        
        // Tracks how many bytes have been downloaded
        unsigned long m_bytes;
        
        // Vector of WprofHTMLTag derived from this resource
        // e.g., WprofHTMLTag of <script> is from an html
        Vector<WprofHTMLTag*>* m_derivedWprofHTMLTagVector;
        
        // The WprofHTMLTag that this resource is made from. There is only one
        // such WprofHTMLTag for each url. Only the page request does not have one.
        WprofHTMLTag* m_fromWprofHTMLTag;
        
        // Info pulled out from ResourceResponseBase.h
        // ResourceLoadTiming
        RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
        String m_mimeType;
        long long m_expectedContentLength;
        int m_httpStatusCode;
        
        unsigned m_connectionId;
        bool m_connectionReused;
        bool m_wasCached;
};
	
}
#endif // WPROF_DISABLED

#endif
