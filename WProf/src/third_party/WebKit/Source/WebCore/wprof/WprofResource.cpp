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

#if !WPROF_DISABLED

#include "WprofResource.h"

namespace WebCore {

  WprofReceivedChunk::WprofReceivedChunk(unsigned long id_url, unsigned long len, double time) {
    m_id = id_url;
    m_len = len;
    m_time = time;
  }
        
  WprofReceivedChunk::~WprofReceivedChunk() {}
        
  unsigned long WprofReceivedChunk::getId() { return m_id; }
  unsigned long WprofReceivedChunk::len() { return m_len; }
  double WprofReceivedChunk::time() { return m_time; }
        

  WprofResource::WprofResource(unsigned long id,
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
			       WprofElement* from)
    : m_bytes(0)
  {
    m_id = id;
    
    // [Note] deep copy
    if (resourceLoadTiming != NULL){
      m_resourceLoadTiming = resourceLoadTiming->deepCopy();
    }
    
    m_url = url;
    m_mimeType = mime;
    m_httpMethod = httpMethod;
    
    m_expectedContentLength = expectedContentLength;
    m_httpStatusCode = httpStatusCode;
    m_connectionId = connectionId;
    m_connectionReused = connectionReused;
    m_wasCached = wasCached;
    m_timeDownloadStart = time;
    m_fromWprofObject = (unsigned long)from;

    // Create WprofReceivedChunk and WprofHTMLTag vectors
    m_receivedChunkInfoVector = new Vector<WprofReceivedChunk*>;
  }
        
  WprofResource::~WprofResource() {}
        
  unsigned long WprofResource::getId() { return m_id; }
  String WprofResource::url() { return m_url; }
  RefPtr<ResourceLoadTiming> WprofResource::resourceLoadTiming() { return m_resourceLoadTiming; }
  String WprofResource::mimeType() { return m_mimeType; }
  long long WprofResource::expectedContentLength() { return m_expectedContentLength; }
  int WprofResource::httpStatusCode() { return m_httpStatusCode; }
  unsigned WprofResource::connectionId() { return m_connectionId; }
  bool WprofResource::connectionReused() { return m_connectionReused; }
  bool WprofResource::wasCached() { return m_wasCached; }
  double WprofResource::timeDownloadStart() { return m_timeDownloadStart; }
  unsigned long WprofResource::fromWprofObject() { return m_fromWprofObject; }
  unsigned long WprofResource::bytes() { return m_bytes; }
  String WprofResource::httpMethod() { return m_httpMethod;}

  Vector<WprofReceivedChunk*>* WprofResource::receivedChunkInfoVector() { return m_receivedChunkInfoVector; }
        
        
        // Called only in WprofController::createWprofReceivedChunk()
  void WprofResource::addBytes(unsigned long bytes) {
    m_bytes += bytes;
  }
        
  // Called only in WprofController::createWprofReceivedChunk()
  void WprofResource::appendWprofReceivedChunk(WprofReceivedChunk* info) {
    if (info == NULL){
      return;
    }
    
    // TODO temp comment out
    //ASSERT(info->getId() == getId());
    m_receivedChunkInfoVector->append(info);
  }
        	
}
#endif // WPROF_DISABLED
