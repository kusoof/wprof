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

#include "WprofElement.h"
#include <stdio.h>
#include <stdlib.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

  WprofElement::WprofElement(WprofPage* page)
  {
    m_startTime = 0;
    m_endTime = 0;
    m_page = page; 
  }
       
  WprofElement::~WprofElement() { 
    
  }
  
  
  double WprofElement::startTime() { return m_startTime; }
  void WprofElement::setStartEndTime (double start, double end) {m_startTime = start; m_endTime = end;}
  double WprofElement::endTime() { return m_endTime;}
  WprofPage* WprofElement::page() {return m_page;}

  String WprofElement::docUrl() { return String();}
  Frame* WprofElement::frame() { return NULL;}
  
  void WprofElement::appendUrl(String url){
    m_urls.append(url);
  }

  WprofElement* WprofElement::parent(){
    return NULL;
  }

  bool WprofElement::isComputation(){
    return false;
  }

  void WprofElement::removeUrl(String url){
    bool found = false;
    size_t i = 0;
    for(; i < m_urls.size(); i++){
      if(url == m_urls[i]){
	found = true;
	break;
      }
    }
    if(found){
      m_urls.remove(i);
    }
  }
    
  /*Vector<String>* WprofElement::urls(){
    return m_urls;
    }*/

  void WprofElement::print(){

    //Get the urls that might be requested by this tag, note that scripts
    //can request multiple resources, like images.		    
    fprintf(stderr, "Error! should call this from subclass");
  }

}
