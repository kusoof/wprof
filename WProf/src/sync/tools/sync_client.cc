// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_event_handler.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/sync_notifier_factory.h"
#include "sync/test/fake_encryptor.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes a sync client and
// prints out any events.

// TODO(akalin): Refactor to combine shared code with
// sync_listen_notifications.
namespace {

const char kEmailSwitch[] = "email";
const char kTokenSwitch[] = "token";
const char kXmppHostPortSwitch[] = "xmpp-host-port";
const char kXmppTrySslTcpFirstSwitch[] = "xmpp-try-ssltcp-first";
const char kXmppAllowInsecureConnectionSwitch[] =
    "xmpp-allow-insecure-connection";
const char kNotificationMethodSwitch[] = "notification-method";

class NullInvalidationStateTracker
    : public base::SupportsWeakPtr<NullInvalidationStateTracker>,
      public syncer::InvalidationStateTracker {
 public:
  NullInvalidationStateTracker() {}
  virtual ~NullInvalidationStateTracker() {}

  virtual syncer::InvalidationVersionMap
      GetAllMaxVersions() const OVERRIDE {
    return syncer::InvalidationVersionMap();
  }

  virtual void SetMaxVersion(
      const invalidation::ObjectId& id,
      int64 max_invalidation_version) OVERRIDE {
    VLOG(1) << "Setting max invalidation version for "
            << syncer::ObjectIdToString(id) << " to "
            << max_invalidation_version;
  }

  virtual std::string GetInvalidationState() const OVERRIDE {
    return std::string();
  }

  virtual void SetInvalidationState(const std::string& state) OVERRIDE {
    std::string base64_state;
    CHECK(base::Base64Encode(state, &base64_state));
    VLOG(1) << "Setting invalidation state to: " << base64_state;
  }
};

// Needed to use a real host resolver.
class MyTestURLRequestContext : public TestURLRequestContext {
 public:
  MyTestURLRequestContext() : TestURLRequestContext(true) {
    context_storage_.set_host_resolver(
        net::CreateSystemHostResolver(
            net::HostResolver::kDefaultParallelism,
            net::HostResolver::kDefaultRetryAttempts,
            NULL));
    Init();
  }

  virtual ~MyTestURLRequestContext() {}
};

class MyTestURLRequestContextGetter : public TestURLRequestContextGetter {
 public:
  explicit MyTestURLRequestContextGetter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy)
      : TestURLRequestContextGetter(io_message_loop_proxy) {}

  virtual TestURLRequestContext* GetURLRequestContext() OVERRIDE {
    // Construct |context_| lazily so it gets constructed on the right
    // thread (the IO thread).
    if (!context_.get())
      context_.reset(new MyTestURLRequestContext());
    return context_.get();
  }

 private:
  virtual ~MyTestURLRequestContextGetter() {}

  scoped_ptr<MyTestURLRequestContext> context_;
};

// TODO(akalin): Use system encryptor once it's moved to sync/.
class NullEncryptor : public syncer::Encryptor {
 public:
  virtual ~NullEncryptor() {}

  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE {
    *ciphertext = plaintext;
    return true;
  }

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE {
    *plaintext = ciphertext;
    return true;
  }
};

std::string ValueToString(const Value& value) {
  std::string str;
  base::JSONWriter::Write(&value, &str);
  return str;
}

class LoggingChangeDelegate : public syncer::SyncManager::ChangeDelegate {
 public:
  virtual ~LoggingChangeDelegate() {}

  virtual void OnChangesApplied(
      syncable::ModelType model_type,
      const syncer::BaseTransaction* trans,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE {
    LOG(INFO) << "Changes applied for "
              << syncable::ModelTypeToString(model_type);
    size_t i = 1;
    size_t change_count = changes.Get().size();
    for (syncer::ChangeRecordList::const_iterator it =
             changes.Get().begin(); it != changes.Get().end(); ++it) {
      scoped_ptr<base::DictionaryValue> change_value(it->ToValue());
      LOG(INFO) << "Change (" << i << "/" << change_count << "): "
                << ValueToString(*change_value);
      if (it->action != syncer::ChangeRecord::ACTION_DELETE) {
        syncer::ReadNode node(trans);
        CHECK_EQ(node.InitByIdLookup(it->id), syncer::BaseNode::INIT_OK);
        scoped_ptr<base::DictionaryValue> details(node.GetDetailsAsValue());
        VLOG(1) << "Details: " << ValueToString(*details);
      }
      ++i;
    }
  }

  virtual void OnChangesComplete(syncable::ModelType model_type) OVERRIDE {
    LOG(INFO) << "Changes complete for "
              << syncable::ModelTypeToString(model_type);
  }
};

class LoggingUnrecoverableErrorHandler
    : public syncer::UnrecoverableErrorHandler {
 public:
  virtual ~LoggingUnrecoverableErrorHandler() {}

  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE {
    if (LOG_IS_ON(ERROR)) {
      logging::LogMessage(from_here.file_name(), from_here.line_number(),
                          logging::LOG_ERROR).stream()
          << message;
    }
  }
};

class LoggingJsEventHandler
    : public syncer::JsEventHandler,
      public base::SupportsWeakPtr<LoggingJsEventHandler> {
 public:
  virtual ~LoggingJsEventHandler() {}

  virtual void HandleJsEvent(
      const std::string& name,
      const syncer::JsEventDetails& details) OVERRIDE {
    VLOG(1) << name << ": " << details.ToString();
  }
};

void LogUnrecoverableErrorContext() {
  base::debug::StackTrace stack_trace;
  stack_trace.PrintBacktrace();
}

notifier::NotifierOptions ParseNotifierOptions(
    const CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>&
        request_context_getter) {
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;

  if (command_line.HasSwitch(kXmppHostPortSwitch)) {
    notifier_options.xmpp_host_port =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(kXmppHostPortSwitch));
    LOG(INFO) << "Using " << notifier_options.xmpp_host_port.ToString()
              << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(kXmppTrySslTcpFirstSwitch);
  LOG_IF(INFO, notifier_options.try_ssltcp_first)
      << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(kXmppAllowInsecureConnectionSwitch);
  LOG_IF(INFO, notifier_options.allow_insecure_connection)
      << "Allowing insecure XMPP connections.";

  if (command_line.HasSwitch(kNotificationMethodSwitch)) {
    notifier_options.notification_method =
        notifier::StringToNotificationMethod(
            command_line.GetSwitchValueASCII(kNotificationMethodSwitch));
  }

  return notifier_options;
}

}  // namespace

int main(int argc, char* argv[]) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  MessageLoop sync_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  syncer::SyncCredentials credentials;
  credentials.email = command_line.GetSwitchValueASCII(kEmailSwitch);
  credentials.sync_token = command_line.GetSwitchValueASCII(kTokenSwitch);
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (credentials.email.empty() || credentials.sync_token.empty()) {
    std::printf("Usage: %s --%s=foo@bar.com --%s=token\n"
                "[--%s=host:port] [--%s] [--%s]\n"
                "[--%s=(server|p2p)]\n\n"
                "Run chrome and set a breakpoint on\n"
                "syncer::SyncManager::SyncInternal::UpdateCredentials() "
                "after logging into\n"
                "sync to get the token to pass into this utility.\n",
                argv[0],
                kEmailSwitch, kTokenSwitch, kXmppHostPortSwitch,
                kXmppTrySslTcpFirstSwitch,
                kXmppAllowInsecureConnectionSwitch,
                kNotificationMethodSwitch);
    return -1;
  }

  // Set up objects that monitor the network.
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::Create());

  // Set up sync notifier factory.
  const scoped_refptr<MyTestURLRequestContextGetter> context_getter =
      new MyTestURLRequestContextGetter(io_thread.message_loop_proxy());
  const notifier::NotifierOptions& notifier_options =
      ParseNotifierOptions(command_line, context_getter);
  const char kClientInfo[] = "sync_listen_notifications";
  NullInvalidationStateTracker null_invalidation_state_tracker;
  syncer::SyncNotifierFactory sync_notifier_factory(
      notifier_options, kClientInfo,
      null_invalidation_state_tracker.AsWeakPtr());

  // Set up database directory for the syncer.
  ScopedTempDir database_dir;
  CHECK(database_dir.CreateUniqueTempDir());

  // Set up model type parameters.
  const syncable::ModelTypeSet model_types = syncable::ModelTypeSet::All();
  syncer::ModelSafeRoutingInfo routing_info;
  for (syncable::ModelTypeSet::Iterator it = model_types.First();
       it.Good(); it.Inc()) {
    routing_info[it.Get()] = syncer::GROUP_PASSIVE;
  }
  scoped_refptr<syncer::PassiveModelWorker> passive_model_safe_worker =
      new syncer::PassiveModelWorker(&sync_loop);
  std::vector<syncer::ModelSafeWorker*> workers;
  workers.push_back(passive_model_safe_worker.get());

  // Set up sync manager.
  syncer::SyncManager sync_manager("sync_client manager");
  LoggingJsEventHandler js_event_handler;
  const char kSyncServerAndPath[] = "clients4.google.com/chrome-sync/dev";
  int kSyncServerPort = 443;
  bool kUseSsl = true;
  // Used only by RefreshNigori(), so it's okay to leave this as NULL.
  const scoped_refptr<base::TaskRunner> blocking_task_runner = NULL;
  const char kUserAgent[] = "sync_client";
  // TODO(akalin): Replace this with just the context getter once
  // HttpPostProviderFactory is removed.
  scoped_ptr<syncer::HttpPostProviderFactory> post_factory(
      new syncer::HttpBridgeFactory(context_getter, kUserAgent));
  // Used only when committing bookmarks, so it's okay to leave this
  // as NULL.
  syncer::ExtensionsActivityMonitor* extensions_activity_monitor = NULL;
  LoggingChangeDelegate change_delegate;
  const char kRestoredKeyForBootstrapping[] = "";
  const syncer::SyncManager::TestingMode kTestingMode =
      syncer::SyncManager::NON_TEST;
  NullEncryptor null_encryptor;
  LoggingUnrecoverableErrorHandler unrecoverable_error_handler;
  sync_manager.Init(database_dir.path(),
                    syncer::WeakHandle<syncer::JsEventHandler>(
                        js_event_handler.AsWeakPtr()),
                    kSyncServerAndPath,
                    kSyncServerPort,
                    kUseSsl,
                    blocking_task_runner,
                    post_factory.release(),
                    routing_info,
                    workers,
                    extensions_activity_monitor,
                    &change_delegate,
                    credentials,
                    sync_notifier_factory.CreateSyncNotifier(),
                    kRestoredKeyForBootstrapping,
                    kTestingMode,
                    &null_encryptor,
                    &unrecoverable_error_handler,
                    &LogUnrecoverableErrorContext);
  // TODO(akalin): We have pass in model parameters multiple times.
  // Organize handling of model types.
  sync_manager.UpdateEnabledTypes(model_types);
  sync_manager.StartSyncingNormally(routing_info);

  sync_loop.Run();

  io_thread.Stop();
  return 0;
}
