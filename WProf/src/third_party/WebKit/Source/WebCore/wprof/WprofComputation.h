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


  class WprofComputation : public WprofElement {
  public:
    WprofComputation(int type, WprofElement* element, WprofPage* page);      
    ~WprofComputation();
        
    int type();
    WprofElement* fromWprofElement();
    String urlRecalcStyle();
        
    void setUrlRecalcStyle(String url);
    void end();
        
    String getTypeForPrint();

    void print();
        
  protected:
    int m_type; // 1: recalcStyle; 2: layout; 3: paint
    double m_startTime;
    double m_endTime;
    WprofElement* m_fromWprofElement;
    String m_urlRecalcStyle;
};
	
}
#endif // WPROF_DISABLED

#endif
