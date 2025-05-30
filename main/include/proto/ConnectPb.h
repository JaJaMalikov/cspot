#pragma once

#include "NanoPBHelper.h"

// Protobuf includes
#include "connect.pb.h"

namespace cspot_proto {
struct Capabilities {
  std::vector<std::string> supportedTypes;

  // Capabilities is mostly bools, so we can reuse the proto struct
  _Capabilities rawProto = Capabilities_init_zero;

  static auto& bindFields(Capabilities* self, bool isDecode) {
    // Only custom bind the supported types
    nanopb_helper::bindField(self->rawProto.supported_types,
                             self->supportedTypes, isDecode);

    return self->rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::Capabilities, Capabilities_fields)

namespace cspot_proto {
struct DeviceInfo {
  bool canPlay;
  uint32_t volume;
  std::string name;
  DeviceType deviceType;
  std::string deviceSoftwareVersion;
  std::string deviceId;
  std::string clientId;
  std::string spircVersion;
  cspot_proto::Capabilities capabilities;

  static auto bindFields(DeviceInfo* self, bool isDecode) {
    _DeviceInfo rawProto = DeviceInfo_init_zero;
    nanopb_helper::bindField(rawProto.can_play, self->canPlay, isDecode);
    nanopb_helper::bindVarintField(rawProto.volume, self->volume, isDecode);
    nanopb_helper::bindField(rawProto.name, self->name, isDecode);
    nanopb_helper::bindVarintField(rawProto.device_type, self->deviceType,
                                   isDecode);
    nanopb_helper::bindField(rawProto.device_software_version,
                             self->deviceSoftwareVersion, isDecode);
    nanopb_helper::bindField(rawProto.device_id, self->deviceId, isDecode);
    nanopb_helper::bindField(rawProto.client_id, self->clientId, isDecode);
    nanopb_helper::bindField(rawProto.spirc_version, self->spircVersion,
                             isDecode);
    nanopb_helper::bindField(rawProto.capabilities, self->capabilities,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::DeviceInfo, DeviceInfo_fields)

namespace cspot_proto {
struct ContextTrack {
  std::string uri;
  std::string uid;
  std::vector<uint8_t> gid;

  static auto bindFields(ContextTrack* self, bool isDecode) {
    _ContextTrack rawProto = ContextTrack_init_zero;
    nanopb_helper::bindField(rawProto.uri, self->uri, isDecode);
    nanopb_helper::bindField(rawProto.uid, self->uid, isDecode);
    nanopb_helper::bindField(rawProto.gid, self->gid, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::ContextTrack, ContextTrack_fields)

namespace cspot_proto {
struct ContextPage {
  std::string pageUrl;
  std::string nextPageUrl;
  std::vector<cspot_proto::ContextTrack> tracks;

  static auto bindFields(ContextPage* self, bool isDecode) {
    _ContextPage rawProto = ContextPage_init_zero;
    nanopb_helper::bindField(rawProto.page_url, self->pageUrl, isDecode);
    nanopb_helper::bindField(rawProto.next_page_url, self->nextPageUrl,
                             isDecode);
    nanopb_helper::bindField(rawProto.tracks, self->tracks, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto
NANOPB_STRUCT(cspot_proto::ContextPage, ContextPage_fields)

namespace cspot_proto {
struct ContextIndex {
  int32_t page = 0;
  int32_t track = 0;

  static auto bindFields(ContextIndex* self, bool isDecode) {
    _ContextIndex rawProto = ContextIndex_init_zero;
    nanopb_helper::bindVarintField(rawProto.page, self->page, isDecode);
    nanopb_helper::bindVarintField(rawProto.track, self->track, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto
NANOPB_STRUCT(cspot_proto::ContextIndex, ContextIndex_fields)

namespace cspot_proto {
struct Context {
  std::string uri;
  std::string url;
  std::vector<cspot_proto::ContextPage> pages;
  bool loading;

  static auto bindFields(Context* self, bool isDecode) {
    _Context rawProto = Context_init_zero;
    nanopb_helper::bindField(rawProto.uri, self->uri, isDecode);
    nanopb_helper::bindField(rawProto.url, self->url, isDecode);
    nanopb_helper::bindField(rawProto.pages, self->pages, isDecode);
    nanopb_helper::bindField(rawProto.loading, self->loading, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto
NANOPB_STRUCT(cspot_proto::Context, Context_fields)

namespace cspot_proto {
struct ContextPlayerOptions {
  bool shufflingContext;
  bool repeatingContext;
  bool repeatingTrack;
  nanopb_helper::Optional<float> playbackSpeed;

  static auto bindFields(ContextPlayerOptions* self, bool isDecode) {
    _ContextPlayerOptions rawProto = ContextPlayerOptions_init_zero;
    nanopb_helper::bindField(rawProto.shuffling_context, self->shufflingContext,
                             isDecode);
    nanopb_helper::bindField(rawProto.repeating_context, self->repeatingContext,
                             isDecode);
    nanopb_helper::bindField(rawProto.repeating_track, self->repeatingTrack,
                             isDecode);
    nanopb_helper::bindField(rawProto.playback_speed, self->playbackSpeed,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::ContextPlayerOptions, ContextPlayerOptions_fields)

namespace cspot_proto {
struct ProvidedTrack {
  std::string uri;
  std::string uid;
  std::string provider;

  static auto bindFields(ProvidedTrack* self, bool isDecode) {
    _ProvidedTrack rawProto = ProvidedTrack_init_zero;
    nanopb_helper::bindField(rawProto.uri, self->uri, isDecode);
    nanopb_helper::bindField(rawProto.uid, self->uid, isDecode);
    nanopb_helper::bindField(rawProto.provider, self->provider, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::ProvidedTrack, ProvidedTrack_fields)

namespace cspot_proto {
struct PlayerState {
  int64_t timestamp = 0;
  std::string contextUri;
  std::string contextUrl;
  cspot_proto::ProvidedTrack track;
  std::string playbackId;
  nanopb_helper::Optional<ContextIndex> index;
  double playbackSpeed = 1.0;
  int64_t positionAsOfTimestamp = 0;
  int64_t duration = 0;
  bool isPlaying;
  bool isBuffering;
  bool isPaused;
  bool isSystemInitiated;
  std::vector<cspot_proto::ProvidedTrack> nextTracks;
  std::vector<cspot_proto::ProvidedTrack> prevTracks;
  std::string sessionId;
  int64_t position = 0;

  static auto bindFields(PlayerState* self, bool isDecode) {
    _PlayerState rawProto = PlayerState_init_zero;
    nanopb_helper::bindVarintField(rawProto.timestamp, self->timestamp,
                                   isDecode);
    nanopb_helper::bindField(rawProto.context_uri, self->contextUri, isDecode);
    nanopb_helper::bindField(rawProto.context_url, self->contextUrl, isDecode);
    nanopb_helper::bindField(rawProto.track, self->track, isDecode);
    nanopb_helper::bindField(rawProto.playback_id, self->playbackId, isDecode);
    nanopb_helper::bindField(rawProto.index, self->index, isDecode);
    nanopb_helper::bindField(rawProto.playback_speed, self->playbackSpeed,
                             isDecode);
    nanopb_helper::bindVarintField(rawProto.position_as_of_timestamp,
                                   self->positionAsOfTimestamp, isDecode);
    nanopb_helper::bindVarintField(rawProto.duration, self->duration, isDecode);
    nanopb_helper::bindField(rawProto.is_playing, self->isPlaying, isDecode);
    nanopb_helper::bindField(rawProto.is_buffering, self->isBuffering,
                             isDecode);
    nanopb_helper::bindField(rawProto.is_paused, self->isPaused, isDecode);
    nanopb_helper::bindField(rawProto.is_system_initiated,
                             self->isSystemInitiated, isDecode);
    nanopb_helper::bindField(rawProto.next_tracks, self->nextTracks, isDecode);
    nanopb_helper::bindField(rawProto.prev_tracks, self->prevTracks, isDecode);
    nanopb_helper::bindField(rawProto.session_id, self->sessionId, isDecode);
    nanopb_helper::bindVarintField(rawProto.position, self->position, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::PlayerState, PlayerState_fields)

namespace cspot_proto {
struct Playback {
  int64_t timestamp;
  int64_t positionAsOfTimestamp;
  double playbackSpeed;
  bool isPaused;
  cspot_proto::ContextTrack currentTrack;

  static auto bindFields(Playback* self, bool isDecode) {
    _Playback rawProto = Playback_init_zero;
    nanopb_helper::bindVarintField(rawProto.timestamp, self->timestamp,
                                   isDecode);
    nanopb_helper::bindVarintField(rawProto.position_as_of_timestamp,
                                   self->positionAsOfTimestamp, isDecode);
    nanopb_helper::bindField(rawProto.playback_speed, self->playbackSpeed,
                             isDecode);
    nanopb_helper::bindField(rawProto.is_paused, self->isPaused, isDecode);
    nanopb_helper::bindField(rawProto.current_track, self->currentTrack,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::Playback, Playback_fields)

namespace cspot_proto {
struct Queue {
  std::vector<cspot_proto::ContextTrack> tracks;
  bool isPlayingQueue;

  static auto bindFields(Queue* self, bool isDecode) {
    _Queue rawProto = Queue_init_zero;
    nanopb_helper::bindField(rawProto.tracks, self->tracks, isDecode);
    nanopb_helper::bindField(rawProto.is_playing_queue, self->isPlayingQueue,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::Queue, Queue_fields)

namespace cspot_proto {
struct Session {
  cspot_proto::Context context;
  std::string currentUid;

  static auto bindFields(Session* self, bool isDecode) {
    _Session rawProto = Session_init_zero;
    nanopb_helper::bindField(rawProto.context, self->context, isDecode);
    nanopb_helper::bindField(rawProto.current_uid, self->currentUid, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::Session, Session_fields)

namespace cspot_proto {
struct Device {
  cspot_proto::DeviceInfo deviceInfo;
  cspot_proto::PlayerState playerState;

  static auto bindFields(Device* self, bool isDecode) {
    _Device rawProto = Device_init_zero;
    nanopb_helper::bindField(rawProto.device_info, self->deviceInfo, isDecode);
    nanopb_helper::bindField(rawProto.player_state, self->playerState,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::Device, Device_fields)

namespace cspot_proto {
struct PutStateRequest {
  cspot_proto::Device device;
  MemberType memberType;
  bool isActive = false;
  PutStateReason putStateReason;
  uint32_t mesasgeId = 0;
  std::string lastCommandSentByDeviceId;
  uint32_t lastCommandMessageId = 0;
  uint64_t startedPlayingAt = 0;
  uint64_t hasBeenPlayingForMs = 0;
  uint64_t clientSideTimestamp = 0;
  bool onlyWritePlayerState = false;

  static auto bindFields(PutStateRequest* self, bool isDecode) {
    _PutStateRequest rawProto = PutStateRequest_init_zero;
    nanopb_helper::bindField(rawProto.device, self->device, isDecode);
    nanopb_helper::bindVarintField(rawProto.member_type, self->memberType,
                                   isDecode);
    nanopb_helper::bindField(rawProto.is_active, self->isActive, isDecode);
    nanopb_helper::bindVarintField(rawProto.put_state_reason,
                                   self->putStateReason, isDecode);
    nanopb_helper::bindVarintField(rawProto.message_id, self->mesasgeId,
                                   isDecode);
    nanopb_helper::bindField(rawProto.last_command_sent_by_device_id,
                             self->lastCommandSentByDeviceId, isDecode);
    nanopb_helper::bindVarintField(rawProto.last_command_message_id,
                                   self->lastCommandMessageId, isDecode);
    nanopb_helper::bindVarintField(rawProto.started_playing_at,
                                   self->startedPlayingAt, isDecode);
    nanopb_helper::bindVarintField(rawProto.has_been_playing_for_ms,
                                   self->hasBeenPlayingForMs, isDecode);
    nanopb_helper::bindVarintField(rawProto.client_side_timestamp,
                                   self->clientSideTimestamp, isDecode);
    nanopb_helper::bindField(rawProto.only_write_player_state,
                             self->onlyWritePlayerState, isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::PutStateRequest, PutStateRequest_fields)

namespace cspot_proto {
struct TransferState {
  cspot_proto::ContextPlayerOptions options;
  cspot_proto::Playback playback;
  cspot_proto::Session current_session;
  cspot_proto::Queue queue;

  static auto bindFields(TransferState* self, bool isDecode) {
    _TransferState rawProto = TransferState_init_zero;
    nanopb_helper::bindField(rawProto.options, self->options, isDecode);
    nanopb_helper::bindField(rawProto.playback, self->playback, isDecode);
    nanopb_helper::bindField(rawProto.queue, self->queue, isDecode);
    nanopb_helper::bindField(rawProto.current_session, self->current_session,
                             isDecode);
    return rawProto;
  }
};
}  // namespace cspot_proto

NANOPB_STRUCT(cspot_proto::TransferState, TransferState_fields)
