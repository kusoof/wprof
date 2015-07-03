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

#ifndef WprofComputation_h
#define WprofComputation_h

#if !WPROF_DISABLED

#include "WprofElement.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

  class Frame;

  typedef enum {
    ComputationRecalcStyle = 1,
    ComputationLayout = 2,
    ComputationPaint = 3,
    ComputationExecScript = 4,
    ComputationFireEvent= 5,
    ComputationTimer = 6
  } WprofComputationType;
  
  class WprofComputation : public WprofElement {
  public:
    WprofComputation(WprofComputationType type, WprofElement* element, WprofPage* page);      
    ~WprofComputation();
        
    virtual WprofComputationType type();
    WprofElement* fromWprofElement();
    String urlRecalcStyle();
        
    void setUrlRecalcStyle(String url);
    virtual void end();
        
    String getTypeForPrint();

    virtual void print();
    virtual bool isComputation();
    virtual WprofElement* parent();

    bool isRenderType(){
      return (m_type == ComputationPaint) || (m_type == ComputationLayout);
    }

    virtual String docUrl();
    virtual unsigned long frameId();
        
  protected:
    WprofComputationType m_type; // 1: recalcStyle; 2: layout; 3: paint
    double m_startTime;
    double m_endTime;
    WprofElement* m_fromWprofElement;
    String m_urlRecalcStyle;
  };

  typedef enum {
    EventTargetOther = 0,
    EventTargetElement = 1,
    EventTargetWindow,
    EventTargetDocument,
    EventTargetXMLHTTPRequest,
    EventTargetMessagePort
  } WprofEventTargetType;

  class WprofEvent : public WprofComputation {
  public:
    WprofEvent (String name, WprofElement* target, WprofEventTargetType targetType, String info, String docUrl, Frame* frame,  WprofPage* page);
    WprofEvent (String name, WprofElement* target, WprofEventTargetType targetType, String docUrl, Frame* frame, WprofPage* page);

    void print();
    WprofComputationType type () {return ComputationFireEvent;}

    WprofElement* target();
    WprofEventTargetType targetType();
    String info();
    unsigned long frameId();
    String EventName();

  protected:
    WprofEventTargetType m_targetType;
    String m_info;
    String m_docUrl;
    unsigned long m_frameId;

    //Helper Function
    String targetTypeString();
    
  };
	
}
#endif // WPROF_DISABLED

#endif
