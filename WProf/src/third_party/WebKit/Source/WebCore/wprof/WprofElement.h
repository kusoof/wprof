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

#ifndef WprofElement_h
#define WprofElement_h

#if !WPROF_DISABLED

#include "config.h"

#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>
#include <string.h>

namespace WebCore {

  class WprofPage;
  class Frame;

  // Define a hash of an object (we use its text position)
  // All WprofHTMLTag instances are created once, this means that we can
  // just use its pointer as the key to retrieve it.
  class WprofElement {
  public:
    WprofElement(WprofPage* page);
        
    virtual ~WprofElement();
    virtual bool operator==(WprofElement& element){return (this == &element);}
    
    double startTime();
    double endTime();
    virtual void setStartEndTime (double start, double end);
    
    WprofPage* page();
    
    void appendUrl(String url);
    void removeUrl(String url);

    virtual void print();
    virtual String docUrl();

    virtual Frame* frame();

    virtual bool isComputation();
    virtual WprofElement* parent();
    
  protected:    

    double m_startTime;
    double m_endTime;
    Vector<String> m_urls;
    WprofPage* m_page;

  };

}
#endif // WPROF_DISABLED

#endif
