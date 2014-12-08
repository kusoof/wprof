/*
 * WprofPreload.h
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

#ifndef WprofPreload_h
#define WprofPreload_h

#if !WPROF_DISABLED

#include "WprofHTMLTag.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>

namespace WebCore {

// Record speculative loaded requests based on HTMLPreloadScanner
class WprofPreload {
    public:
 WprofPreload(WprofHTMLTag* tag,
	      String url,
	      String docUrl,
	      String tagName,
	      int line,
	      int column):
  m_fromWprofHTMLTag(0),
    m_url(url),
    m_docUrl(docUrl)
        {
            m_executingScriptTag = tag;
            m_time = monotonicallyIncreasingTime();
	    m_line = line;
	    m_column = column;
	    m_tagName = tagName;
        };
		
        ~WprofPreload() {};
        
        String url() { return m_url; }
        WprofHTMLTag* fromWprofHTMLTag() { return m_fromWprofHTMLTag; }
	WprofHTMLTag* executingScriptTag() { return m_executingScriptTag;}

	void setFromTag(WprofHTMLTag* tag) { m_fromWprofHTMLTag = tag;}
	
        double time() { return m_time; }
	String docUrl() {return m_docUrl;}
	int line() {return m_line;}
	int column() { return m_column;}
	String tagName() {return m_tagName;}

	bool matchesToken (String docUrl, Vector<String>* urls, String tagName, int line, int column){
	  if (m_docUrl != docUrl) return false;
	  
	  bool matches = (m_tagName == tagName) 
	    && (m_line == line) && (m_column == column);
	  
	  if (!matches){
	    //line number might be inaccurate check the urls
	    for(Vector<String>::iterator it = urls->begin(); it != urls->end(); it++){
	      if(((*it) == m_url) && (m_tagName == tagName)){
		return true;
	      }
	    }
	  }
	  return matches;
	}
        
    private:
        WprofHTMLTag* m_fromWprofHTMLTag;
        String m_url;
        double m_time;
	String m_docUrl;
	String m_tagName;
	int m_line;
	int m_column;
	WprofHTMLTag* m_executingScriptTag;
};
    
}
#endif // WPROF_DISABLED

#endif
