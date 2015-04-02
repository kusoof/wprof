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

#include "config.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <stdio.h>

namespace WebCore {

  class WprofHTMLTag;
  class WprofElement;

// Record speculative loaded requests based on HTMLPreloadScanner
  class WprofPreload {
  public:
    WprofPreload(WprofElement* tag,
		 String url,
		 String docUrl,
		 String tagName,
		 int line,
		 int column);
  		
    ~WprofPreload();
        
    String url();
    WprofHTMLTag* fromWprofHTMLTag();
    WprofElement* executingScriptTag();

    void setFromTag(WprofHTMLTag* tag);
	
    double time();
    String docUrl();
    int line();
    int column();
    String tagName();

    bool matchesToken (String docUrl, String url, String tagName, int line, int column);
    
  private:
    WprofHTMLTag* m_fromWprofHTMLTag;
    String m_url;
    double m_time;
    String m_docUrl;
    String m_tagName;
    int m_line;
    int m_column;
    WprofElement* m_executingScriptTag;
};
    
}
#endif // WPROF_DISABLED

#endif
