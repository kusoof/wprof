/*
 * WprofComputation.h
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

#include "WprofComputation.h"
#include "WprofPage.h"

namespace WebCore {

  WprofComputation::WprofComputation(int type, WprofElement* element, WprofPage* page)
    : WprofElement(page),
      m_endTime(-1),
      m_urlRecalcStyle(emptyString())
  {
    m_type = type;
    m_startTime = monotonicallyIncreasingTime();
    m_fromWprofElement = element;
  };
        
  WprofComputation::~WprofComputation() {};      
  int WprofComputation::type() { return m_type; }
  
  WprofElement* WprofComputation::fromWprofElement() { return m_fromWprofElement; }
  String WprofComputation::urlRecalcStyle() { return m_urlRecalcStyle; }
        
  void WprofComputation::setUrlRecalcStyle(String url) {
    m_urlRecalcStyle = url;
  }
        
  void WprofComputation::end(){
    m_endTime = monotonicallyIncreasingTime();

    //if this is a script execution, notify the page
    if((m_type == 4) || (m_type == 6) || (m_type == 5) || (m_type == 1)){
      m_page->setCurrentComputationComplete();
    }
  }
        
  String WprofComputation::getTypeForPrint() {
    switch (m_type) {
    case 1:
      return String::format("recalcStyle");
    case 2:
      return String::format("layout");
    case 3:
      return String::format("paint");
    case 4:
      return String::format("execScript");
    case 5:
      return String::format("fireEvent");
    case 6:
      return String::format("timer");
    default:
      break;
    }
    return String::format("undefined");
  }

  void WprofComputation::print(){
    fprintf(stderr, "{\"Computation\": {\"type\": \"%s\", \"code\": \"%p\", \"from\": \"%p\",\"docUrl\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urlRecalcStyle\": \"%s\", \"urls\": [ ",
	    getTypeForPrint().utf8().data(),
	    this,
	    m_fromWprofElement,
	    m_fromWprofElement->docUrl().utf8().data(),
	    m_startTime,
	    m_endTime,
	    m_urlRecalcStyle.utf8().data());

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

    fprintf(stderr, " ]}}\n");
  }

}
#endif
