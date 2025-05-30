#include "proto/MetadataPb.h"

_Track cspot_proto::Track::bindFields(cspot_proto::Track* self, bool isDecode) {
  _Track rawProto = Track_init_zero;
  nanopb_helper::bindField(rawProto.gid, self->gid, isDecode);
  nanopb_helper::bindField(rawProto.name, self->name, isDecode);
  nanopb_helper::bindField(rawProto.duration, self->durationMs, isDecode);
  nanopb_helper::bindField(rawProto.album, self->album, isDecode);
  nanopb_helper::bindField(rawProto.artist, self->artists, isDecode);
  nanopb_helper::bindField(rawProto.restriction, self->restrictions, isDecode);
  nanopb_helper::bindField(rawProto.file, self->audioFiles, isDecode);
  nanopb_helper::bindField(rawProto.alternative, self->alternativeTracks,
                           isDecode);
  return rawProto;
}