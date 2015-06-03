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

#include "WprofGenTag.h"
#include "Frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include "WprofComputation.h"

namespace WebCore {

  WprofGenTag::WprofGenTag(WprofPage* page, Frame* frame, String docUrl, String name)
    : WprofElement(page),
      m_docUrl(docUrl)
  {
    m_name = name;
    m_isFragment = true;
    m_parentComputation = NULL;
    m_startTime = monotonicallyIncreasingTime();
    m_endTime = m_startTime;
    m_frameId = frame->identifier();
  }
       
  WprofGenTag::~WprofGenTag() { 
    //delete m_urls;
  }
  
  String WprofGenTag::docUrl() { return m_docUrl; }
  String WprofGenTag::name() { return m_name; }
  bool WprofGenTag::isFragment() {return m_isFragment;}
  unsigned long WprofGenTag::frameId() { return m_frameId;}
  WprofComputation* WprofGenTag::parentComputation () {return m_parentComputation;}
  void WprofGenTag::setParentComputation (WprofComputation* comp) {m_parentComputation = comp;}

  bool WprofGenTag::matchesPreload(WprofPreload* preload, String url){
    return false;
  }

  WprofElement* WprofGenTag::parent(){
    return parentComputation();
  }

  void WprofGenTag::print(){

    //Get the urls that might be requested by this tag, note that scripts
    //can request multiple resources, like images.		    
    fprintf(stderr, "{\"Element\": {\"code\": \"%p\", \"comp\": \"%p\", \"doc\": \"%s\", \"frame\": \"%ld\", \"name\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urls\":  [ ",
	    this,
	    m_parentComputation,
	    m_docUrl.utf8().data(),
	    m_frameId,
	    m_name.utf8().data(),
	    m_startTime,
	    m_endTime);

    size_t numUrls = m_urls.size();
    size_t i = 0;
    for(Vector<String>::iterator it = m_urls.begin(); it!= m_urls.end(); it++){
      if (i == (numUrls -1)){
	fprintf(stderr, "\"%s\"", it->utf8().data());
      }
      else{
	fprintf(stderr, "\"%s\", ", it->utf8().data());
      }
      i++;
    }

    fprintf(stderr, " ], \"isFragment\": %d}}\n", m_isFragment);
  }

}
