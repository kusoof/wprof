/*
 * WprofReceivedChunk.h
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

#ifndef WprofReceivedChunk_h
#define WprofReceivedChunk_h

#if !WPROF_DISABLED

namespace WebCore {

// Define WprofReceivedChunk
// Note: Created only in ResourceLoader::didReceiveData()
//
// This can be retrieved in WprofResource
class WprofReceivedChunk {
    public:
        WprofReceivedChunk(unsigned long id_url, unsigned long len, double time) {
            m_id = id_url;
            m_len = len;
            m_time = time;
        };
        
        ~WprofReceivedChunk() {};
        
        unsigned long getId() { return m_id; };
        unsigned long len() { return m_len; };
        double time() { return m_time; };
        
    private:
        unsigned long m_id; // Matched with WprofResource::m_id, so this could be not unique
        unsigned long m_len; // Length of received data
        double m_time; // Timestamp
};

}
#endif // WPROF_DISABLED

#endif
