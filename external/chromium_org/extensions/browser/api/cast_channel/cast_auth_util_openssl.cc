// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include "base/logging.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const std::string& peer_cert) {
  NOTREACHED();
  return AuthResult();
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
