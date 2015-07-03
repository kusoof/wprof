#include "WprofCachedResource.h"
#include <stdio.h>
#include "wtf/text/WTFString.h"
#include "wtf/text/CString.h"
#include "WprofElement.h"

namespace WebCore {



  WprofCachedResource::WprofCachedResource(unsigned long id,
					   String url,
					   double time,
					   String mimeType,
					   unsigned size,
					   String httpMethod,
					   unsigned long frameId,
					   WprofElement* from):
                                           m_id(id),
					   m_url(url),
					   m_timeCacheAccessed(time),
					   m_fromWprofElement(from),
					   m_mimeType(mimeType),
					   m_size(size),
					   m_httpMethod(httpMethod),
					   m_frameId(frameId)
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

  String WprofCachedResource::mimeType(){
    return m_mimeType;
  }

  unsigned  WprofCachedResource::size(){
    return m_size;
  }

  String WprofCachedResource::httpMethod(){
    return m_httpMethod;
  }

  unsigned long WprofCachedResource::frameId(){
    return m_frameId;
  }

  void WprofCachedResource::print(){
    fprintf(stderr, "{\"Cached\": {\"id\": %ld, \"url\": \"%s\", \"from\": \"%p\", \"mimeType\": \"%s\", \"len\": %u, \"httpMethod\": \"%s\", \"accessTime\": %lf, \"frame\": \"%lu\"}}\n",
	    m_id,
	    m_url.utf8().data(),
	    m_fromWprofElement,
	    m_mimeType.utf8().data(),
	    m_size,
	    m_httpMethod.utf8().data(),
	    m_timeCacheAccessed,
	    m_frameId);
    
  }
	
}
