// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_LOOKUP_REQUEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_LOOKUP_REQUEST_H_
#pragma once

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"

template<class T>
class PepperLookupRequest {
 public:
  typedef base::Callback<void(int, const net::AddressList&, const T&)>
      LookupRequestCallback;

  // Takes ownership over |bound_info|. |bound_info| will be passed to
  // callback, when lookup will finish.
  PepperLookupRequest(net::HostResolver* resolver,
                      const net::HostResolver::RequestInfo& request_info,
                      T* bound_info,
                      const LookupRequestCallback& callback)
      : resolver_(resolver),
        request_info_(request_info),
        bound_info_(bound_info),
        callback_(callback) {
  }

  void Start() {
    int result = resolver_.Resolve(
        request_info_, &addresses_,
        base::Bind(&PepperLookupRequest<T>::OnLookupFinished,
                   base::Unretained(this)),
        net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnLookupFinished(result);
  }

 private:
  void OnLookupFinished(int result) {
    callback_.Run(result, addresses_, *bound_info_);
    delete this;
  }

  net::SingleRequestHostResolver resolver_;
  net::HostResolver::RequestInfo request_info_;
  scoped_ptr<T> bound_info_;
  LookupRequestCallback callback_;

  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(PepperLookupRequest);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_LOOKUP_REQUEST_H_
