
#include "WprofHTMLTag.h"
#include "WprofPreload.h"
#include <stdio.h>
#include <stdlib.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

  // Define a hash of an object (we use its text position)
  // All WprofHTMLTag instances are created once, this means that we can
  // just use its pointer as the key to retrieve it.


  WprofHTMLTag::WprofHTMLTag(WprofPage* page,
			     TextPosition tp,
			     String docUrl,
			     String tag,
			     int pos,
			     bool isFragment,
			     bool isStartTag)
    : WprofGenTag(page, docUrl, tag)
            
  {
    m_textPosition = tp;
    m_startTagEndPos = pos;
    m_isStartTag = isStartTag;
    m_isFragment = isFragment;
  }
        
  WprofHTMLTag::~WprofHTMLTag() { 
    //delete m_urls;
  }
        
  TextPosition WprofHTMLTag::pos() { return m_textPosition; }
  int WprofHTMLTag::startTagEndPos() { return m_startTagEndPos; }
  bool WprofHTMLTag::isStartTag() { return m_isStartTag; }

  void WprofHTMLTag::print(){
    fprintf(stderr, "{\"WprofHTMLTag\": {\"code\": \"%p\", \"comp\": \"%p\", \"doc\": \"%s\", \"row\": %d, \"column\": %d, \"tagName\": \"%s\", \"startTime\": %lf, \"endTime\": %lf, \"urls\":  [ ",
	      this,
	      m_parentComputation,
	      m_docUrl.utf8().data(),
	      m_textPosition.m_line.zeroBasedInt(),
	      m_textPosition.m_column.zeroBasedInt(),
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

      fprintf(stderr, " ], \"pos\": %d, \"isStartTag\": %d, \"isFragment\": %d}}\n",
	      m_startTagEndPos,
	      m_isStartTag,
	      m_isFragment
	      );
    }

  bool WprofHTMLTag::matchesPreload(WprofPreload* preload, String url){
    if (m_docUrl != preload->docUrl()) {
      return false;
    }
    
    bool matches = (m_name == preload->tagName()) 
      && (m_textPosition.m_line.zeroBasedInt() == preload->line()) 
      && (m_textPosition.m_column.zeroBasedInt() == preload->column());
	  
    if (!matches){
      //line number might be inaccurate check the urls
      if((url == preload->url()) && (m_name == preload->tagName())){
	return true;
      }
    }
    return matches;
  }
 
}

