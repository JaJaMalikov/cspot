#pragma once

// System includes
#include <vector>

// Bell includes
#include "bell/Result.h"
#include "pb.h"

// NanoPB includes
#include <pb_common.h>

namespace cspot {
struct ContextTrack {
  std::string uri;
  std::string uid;
  std::vector<uint8_t> gid;
};

struct ProvidedTrack {
  std::string uri;
  std::string uid;
  std::string provider;
};

struct ContextPage {
  std::string page_url;
  std::string next_page_url;
  std::vector<ContextTrack> tracks;
};

// NanoPB encode callback for std::vector<ProvidedTrack>
bool pbEncodeProvidedTrackList(pb_ostream_t* stream, const pb_field_t* field,
                               void* const* arg);

// NanoPB decode callback for std::vector<ContextTrack>
bool pbDecodeContextTrackList(pb_istream_t* stream, const pb_field_t* /*field*/,
                              void** arg);

// NanoPB decode callback for std::vector<ContextPage>
bool pbDecodeContextPageList(pb_istream_t* stream, const pb_field_t* /*field*/,
                             void** arg);

// NanoPB encode callback for std::string
bool pbEncodeString(pb_ostream_t* stream, const pb_field_t* field,
                    void* const* arg);

// NanoPB decode callback for std::string
bool pbDecodeString(pb_istream_t* stream, const pb_field_t* field, void** arg);

// NanoPB encode callback for std::vector<uint8_t>
bool pbEncodeUint8Vector(pb_ostream_t* stream, const pb_field_t* field,
                         void* const* arg);

// NanoPB decode callback for std::vector<uint8_t>
bool pbDecodeUint8Vector(pb_istream_t* stream, const pb_field_t* field,
                         void** arg);

// NanoPB encode callback for std::vector<std::string>
bool pbEncodeStringVector(pb_ostream_t* stream, const pb_field_t* field,
                          void* const* arg);

// NanoPB encode callback for std::optional<bool>
bool pbEncodeOptionalBool(pb_ostream_t* stream, const pb_field_t* field,
                          void* const* arg);

/**
 * @brief Encode a message using NanoPB, and write it to the destination buffer
 *
 * @param buffer Destination buffer
 * @param size Destination buffer size
 * @param messageType NanoPB message definitions
 * @param src Source message
 * @return actual size of the encoded message
 */
bell::Result<size_t> pbEncodeMessage(uint8_t* buffer, size_t size,
                                     const pb_msgdesc_t* messageType,
                                     const void* src);

bell::Result<size_t> pbCalculateEncodedSize(const pb_msgdesc_t* messageType,
                                            const void* src);

/**
 * @brief Decode a message using NanoPB
 *
 * @param buffer Source buffer, containing the encoded message
 * @param size Source buffer size
 * @param messageType NanoPB message definitions
 * @param dest Destination pointer, where the decoded message will be written
 */
bell::Result<> pbDecodeMessage(const uint8_t* buffer, size_t size,
                               const pb_msgdesc_t* messageType, void* dest);
}  // namespace cspot
