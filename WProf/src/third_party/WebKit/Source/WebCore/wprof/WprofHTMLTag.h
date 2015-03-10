/*
 * WprofHTMLTag.h
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

#ifndef WprofHTMLTag_h
#define WprofHTMLTag_h

#if !WPROF_DISABLED

#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>
#include <string.h>

namespace WebCore {

  class WprofPage;
  class WprofComputation;

  // Define a hash of an object (we use its text position)
  // All WprofHTMLTag instances are created once, this means that we can
  // just use its pointer as the key to retrieve it.
  class WprofHTMLTag {
  public:
  WprofHTMLTag(WprofPage* page,
	       TextPosition tp,
	       String docUrl,
	       String tag,
	       int pos,
	       bool isFragment,
	       bool isStartTag)
    : m_docUrl(docUrl),
      m_isUsed(false)
            
        {
	  m_textPosition = tp;
	  m_tagName = tag;
	  m_startTime = 0;
	  m_endTime = 0;
	  m_startTagEndPos = pos;
	  m_isStartTag = isStartTag;
	  m_isFragment = isFragment;
	  m_urls = new Vector<String>();
	  m_page = page;
	  m_parentComputation = NULL;
        }
        
    ~WprofHTMLTag() { 
      //delete m_urls;
    }
		
    bool operator==(WprofHTMLTag other) { return m_textPosition == other.m_textPosition && m_docUrl == other.m_docUrl; }
    bool operator!=(WprofHTMLTag other) { return !((*this) == other); }
        
    TextPosition pos() { return m_textPosition; }
    String docUrl() { return m_docUrl; }
    String tagName() { return m_tagName; }
    double startTime() { return m_startTime; }
    void setStartEndTime (double start, double end) {m_startTime = start; m_endTime = end;}
    double endTime() { return m_endTime;}
    int startTagEndPos() { return m_startTagEndPos; }
    bool isStartTag() { return m_isStartTag; }
    bool isUsed() { return m_isUsed; }
    bool isFragment() {return m_isFragment;}
    WprofPage* page() {return m_page;}
    WprofComputation* parentComputation () {return m_parentComputation;}
    void setParentComputation (WprofComputation* comp) {m_parentComputation = comp;}

    void appendUrl(String url){
      m_urls->append(url);
    }

    void removeUrl(String url){
      bool found = false;
      size_t i = 0;
      for(; i < m_urls->size(); i++){
	if(url == (*m_urls)[i]){
	  found = true;
	  break;
	}
      }
      if(found){
	m_urls->remove(i);
      }
    }
    
    Vector<String>* urls(){
      return m_urls;
    }

    void setUsed() {
      m_isUsed = true;
    }
        
    TextPosition m_textPosition;
    String m_docUrl;
    String m_url;
    String m_tagName;
    double m_startTime;
    double m_endTime;
    int m_startTagEndPos;
    bool m_isStartTag;
    bool m_isUsed;
    bool m_isFragment;
    Vector<String>* m_urls;
    WprofPage* m_page;
    WprofComputation* m_parentComputation;
  };

}
#endif // WPROF_DISABLED

#endif
