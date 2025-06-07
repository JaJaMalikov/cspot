#pragma once

#include "LoginBlob.h"
#include "api/CredentialsResolver.h"
#include "events/EventLoop.h"

namespace cspot {
struct SessionContext {
  std::shared_ptr<LoginBlob> loginBlob;
  std::shared_ptr<EventLoop> eventLoop;
  std::shared_ptr<CredentialsResolver> credentialsResolver;

  std::string sessionId;
};
}  // namespace cspot