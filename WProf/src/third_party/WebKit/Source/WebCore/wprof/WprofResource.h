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
#include "config.h"
#include "ResourceLoadTiming.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

  class WprofComputation;
  class WprofElement;


// Define WprofReceivedChunk
// Note: Created only in ResourceLoader::didReceiveData()
//
// This can be retrieved in WprofResource
  class WprofReceivedChunk {
  public:
    WprofReceivedChunk(unsigned long id_url, unsigned long len, double time);
    ~WprofReceivedChunk();
        
    unsigned long getId();
    unsigned long len();
    double time();

  private:
    unsigned long m_id;
    unsigned long m_len;
    double m_time;
  };

// Define WprofResource
// This is the data structure to store all info about resources
// Note: Created only in ResourceLoader::didReceiveResponse()
//
// This can be retrieved by either a Vector or a HashMap with m_id as the key
class WprofResource {
public:

  WprofResource(unsigned long id,
		String url,
		RefPtr<ResourceLoadTiming> resourceLoadTiming,
		String mime,
		long long expectedContentLength,
		int httpStatusCode,
		String httpMethod,
		unsigned connectionId,
		bool connectionReused,
		bool wasCached,
		double time,
		WprofElement* from);
	        
  ~WprofResource();
        
  unsigned long getId();
  String url();
  RefPtr<ResourceLoadTiming> resourceLoadTiming();
  String mimeType();
  long long expectedContentLength();
  int httpStatusCode();
  unsigned connectionId();
  bool connectionReused();
  bool wasCached();
  double timeDownloadStart();
  unsigned long fromWprofObject();
  unsigned long bytes();
  String httpMethod();

  Vector<WprofReceivedChunk*>* receivedChunkInfoVector();
        
        
  // Called only in WprofController::createWprofReceivedChunk()
  void addBytes(unsigned long bytes);
        
  // Called only in WprofController::createWprofReceivedChunk()
  void appendWprofReceivedChunk(WprofReceivedChunk* info);
        
private:

        unsigned long m_id;
        String m_url;
        double m_timeDownloadStart;
        
        Vector<WprofReceivedChunk*>* m_receivedChunkInfoVector;
        
        // Tracks how many bytes have been downloaded
        unsigned long m_bytes;
        
        
        // The WprofHTMLTag this resource is triggered from. There is only one
        // such object for each url. Only the page request does not have one.
	// Or if the resource is preloaded.
	unsigned long  m_fromWprofObject;
        
        // Info pulled out from ResourceResponseBase.h
        // ResourceLoadTiming
        RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
        String m_mimeType;
        long long m_expectedContentLength;
        int m_httpStatusCode;
        
        unsigned m_connectionId;
        bool m_connectionReused;
        bool m_wasCached;

	String m_httpMethod;
};
	
}
#endif // WPROF_DISABLED

#endif
