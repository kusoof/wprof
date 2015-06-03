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
#include "Frame.h"

namespace WebCore {

  WprofComputation::WprofComputation(WprofComputationType type, WprofElement* element, WprofPage* page)
    : WprofElement(page),
      m_endTime(-1),
      m_urlRecalcStyle(emptyString())
  {
    m_type = type;
    m_startTime = monotonicallyIncreasingTime();
    m_fromWprofElement = element;
  };
        
  WprofComputation::~WprofComputation() {};      
  WprofComputationType WprofComputation::type() { return m_type; }
  
  WprofElement* WprofComputation::fromWprofElement() { return m_fromWprofElement; }
  String WprofComputation::urlRecalcStyle() { return m_urlRecalcStyle; }
        
  void WprofComputation::setUrlRecalcStyle(String url) {
    m_urlRecalcStyle = url;
  }
        
  void WprofComputation::end(){
    m_endTime = monotonicallyIncreasingTime();

    //if this is a script execution, notify the page
    if(!isRenderType()){
      m_page->setCurrentComputationComplete();
    }
  }
        
  String WprofComputation::getTypeForPrint() {
    switch (m_type) {
    case ComputationRecalcStyle:
      return String::format("recalcStyle");
    case ComputationLayout:
      return String::format("layout");
    case ComputationPaint:
      return String::format("paint");
    case ComputationExecScript:
      return String::format("execScript");
    case ComputationFireEvent:
      return String::format("fireEvent");
    case ComputationTimer:
      return String::format("timer");
    default:
      break;
    }
    return String::format("undefined");
  }

  String WprofComputation::docUrl(){
    if(m_fromWprofElement){
      return m_fromWprofElement->docUrl();
    }
    else{
      return m_page->pageURL();
    }
  }

  unsigned long WprofComputation::frameId(){
    if (m_fromWprofElement){
      return m_fromWprofElement->frameId();
    }
    else {
      return 0;
    }
  }

  bool WprofComputation::isComputation(){
    return true;
  }

  WprofElement* WprofComputation::parent(){
    return fromWprofElement();
  }

  void WprofComputation::print(){
    fprintf(stderr, "{\"Computation\": {\"type\": \"%s\", \"code\": \"%p\", \"from\": \"%p\",\"docUrl\": \"%s\", \"frame\": \"%ld\", \"startTime\": %lf, \"endTime\": %lf, \"urlRecalcStyle\": \"%s\", \"urls\": [ ",
	    getTypeForPrint().utf8().data(),
	    this,
	    m_fromWprofElement,
	    m_fromWprofElement->docUrl().utf8().data(),
	    frameId(),
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


  
  
  WprofEvent::WprofEvent (String name,
			  WprofElement* target,
			  WprofEventTargetType targetType,
			  String info,
			  String docUrl,
			  Frame* frame,
			  WprofPage* page): WprofComputation(type(), target, page),
					    m_targetType(targetType),
					    m_info(info),
					    m_docUrl(docUrl),
					    m_frameId(frame->identifier())
  {
    setUrlRecalcStyle(name);
  }
  
  WprofEvent::WprofEvent (String name,
			  WprofElement* target,
			  WprofEventTargetType targetType,
			  String docUrl,
			  Frame* frame,
			  WprofPage* page)
    : WprofComputation(type(), target, page),
      m_targetType(targetType),
      m_info(String()),
      m_docUrl(docUrl),
      m_frameId(frame->identifier())
  {
    setUrlRecalcStyle(name);
  }

  String WprofEvent::targetTypeString(){
    switch(m_targetType){
    case (EventTargetOther):{
      return String::format("Other");
    }
    case(EventTargetElement):{
      return String::format("Element");
    }
    case(EventTargetWindow):{
      return String::format("Window");
    }
    case(EventTargetDocument):{
      return String::format("Document");
    }
    case(EventTargetXMLHTTPRequest):{
      return String::format("XMLHTTPRequest");
    }
    default:{
      return String::format("Other");
    }
    }
  }
    

  void WprofEvent::print(){
    fprintf(stderr, "{\"Computation\": {\"type\": \"%s\", \"code\": \"%p\", \"target\": \"%p\", \"targetType\": \"%s\", \"info\": \"%s\", \"docUrl\": \"%s\", \"frame\": \"%ld\", \"startTime\": %lf, \"endTime\": %lf, \"name\": \"%s\", \"urls\": [ ",
	    getTypeForPrint().utf8().data(),
	    this,
	    m_fromWprofElement,
	    targetTypeString().utf8().data(),
	    m_info.utf8().data(),
	    m_docUrl.utf8().data(),
	    m_frameId,
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
   
  unsigned long WprofEvent::frameId(){
    return m_frameId;
  }

  WprofElement* WprofEvent::target(){
    return m_fromWprofElement;
  }
  
  WprofEventTargetType WprofEvent::targetType(){
    return m_targetType;
  }
  
  String WprofEvent::info(){
    return m_info;
  }
    
  String WprofEvent::EventName(){
    return urlRecalcStyle();
  }
}
#endif
