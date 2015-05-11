#include "WprofCachedResource.h"
#include <stdio.h>
#include "wtf/text/WTFString.h"
#include "wtf/text/CString.h"

namespace WebCore {



  WprofCachedResource::WprofCachedResource(unsigned long id,
					   String url,
					   double time,
					   WprofElement* from):
                                           m_id(id),
					   m_url(url),
					   m_timeCacheAccessed(time),
					   m_fromWprofElement(from)
  {
  }

	        
  WprofCachedResource::~WprofCachedResource()
  {
  }
        
  unsigned long WprofCachedResource::getId()
  {
    return m_id;
  }

  String WprofCachedResource::url()
  {
    return m_url;
  }
  
  double WprofCachedResource::timeCacheAccessed()
  {
    return m_timeCacheAccessed;
  }
  
  WprofElement* WprofCachedResource::fromWprofElement()
  {
    return m_fromWprofElement;
  }

  void WprofCachedResource::print(){
    fprintf(stderr, "{\"Cached\": {\"id\": \"%ld\", \"url\": \"%s\", \"from\": \"%p\", \"accessTime\": %lf}}\n",
	    m_id,
	    m_url.utf8().data(),
	    m_fromWprofElement,
	    m_timeCacheAccessed);
    
  }
	
}
