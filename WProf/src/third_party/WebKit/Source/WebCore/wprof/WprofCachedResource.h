#ifndef WprofCachedResource_h
#define WprofCachedResource_h

#if !WPROF_DISABLED
#include "config.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

  class WprofElement;


// Define WprofResource
// This is the data structure to store a cached resource, with the 
// identifier that maps to the original resource about resources
//

class WprofCachedResource {
public:

  WprofCachedResource(unsigned long id,
		      String url,
		      double time,
		      String mimeType,
		      unsigned size,
		      String httpMethod,
		      unsigned long frameId,
		      WprofElement* from);
	        
  ~WprofCachedResource();
        
  unsigned long getId();
  String url();
  String mimeType();
  String httpMethod();
  unsigned size();
  double timeCacheAccessed();
  WprofElement* fromWprofElement();
  unsigned long frameId();

  void print();
        
private:

        unsigned long m_id;
        String m_url;
        double m_timeCacheAccessed;
        
        
        // The WprofHTMLTag this resource is triggered from. There is only one
        // such object for each url. Only the page request does not have one.
	// Or if the resource is preloaded.
	WprofElement*  m_fromWprofElement;

	String m_mimeType;
        unsigned m_size;
	String m_httpMethod;
	unsigned long m_frameId;
};
	
}
#endif // WPROF_DISABLED

#endif
