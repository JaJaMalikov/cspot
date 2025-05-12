#include "NanoPBExtensions.h"

// System includes
#include <optional>
#include <string>
#include <vector>
#include "pb.h"
#include "pb_decode.h"

// NanoPB includes
#include <pb_encode.h>

using namespace cspot;

bool cspot::pbEncodeString(pb_ostream_t* stream, const pb_field_t* field,
                           void* const* arg) {
  auto& str = *static_cast<std::string*>(*arg);

  if (!str.empty()) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }

    if (!pb_encode_string(stream, reinterpret_cast<pb_byte_t*>(str.data()),
                          str.size())) {
      return false;
    }
  }

  return true;
}

bool cspot::pbEncodeStringVector(pb_ostream_t* stream, const pb_field_t* field,
                                 void* const* arg) {
  auto& vector = *static_cast<std::vector<std::string>*>(*arg);

  for (const auto& str : vector) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }

    if (!pb_encode_string(stream,
                          reinterpret_cast<const pb_byte_t*>(str.data()),
                          str.size())) {
      return false;
    }
  }

  return true;
}

bool cspot::pbDecodeString(pb_istream_t* stream, const pb_field_t* field,
                           void** arg) {
  auto& str = *static_cast<std::string*>(*arg);

  str.resize(stream->bytes_left);

  return pb_read(stream, reinterpret_cast<uint8_t*>(str.data()),
                 stream->bytes_left);
}

bool cspot::pbEncodeOptionalBool(pb_ostream_t* stream, const pb_field_t* field,
                                 void* const* arg) {
  auto& boolean = *static_cast<std::optional<bool>*>(*arg);

  if (boolean.has_value()) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }

    if (!pb_encode_varint(stream, boolean.value())) {
      return false;
    }
  }

  return true;
}

bool cspot::pbEncodeUint8Vector(pb_ostream_t* stream, const pb_field_t* field,
                                void* const* arg) {
  auto& vector = *static_cast<std::vector<uint8_t>*>(*arg);

  if (!vector.empty()) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }

    if (!pb_encode_string(stream, vector.data(), vector.size())) {
      return false;
    }
  }

  return true;
}

bell::Result<size_t> cspot::pbEncodeMessage(uint8_t* buffer, size_t size,
                                            const pb_msgdesc_t* messageType,
                                            const void* src) {
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, size);
  if (!pb_encode(&stream, messageType, src)) {
    return std::errc::bad_message;
  }

  return stream.bytes_written;
}

bell::Result<size_t> cspot::pbCalculateEncodedSize(
    const pb_msgdesc_t* messageType, const void* src) {
  size_t size = 0;
  if (!pb_get_encoded_size(&size, messageType, src)) {
    return std::errc::bad_message;
  }
  return size;
}

bell::Result<> cspot::pbDecodeMessage(const uint8_t* buffer, size_t size,
                                      const pb_msgdesc_t* messageType,
                                      void* dest) {
  pb_istream_t stream = pb_istream_from_buffer(buffer, size);
  bool res = pb_decode(&stream, messageType, dest);

  if (!res) {
    return std::errc::bad_message;
  }

  return {};
}
