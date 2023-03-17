/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7 */

#ifndef PB_MESHTASTIC_MESHTASTIC_CHANNEL_PB_H_INCLUDED
#define PB_MESHTASTIC_MESHTASTIC_CHANNEL_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
/* How this channel is being used (or not).
 Note: this field is an enum to give us options for the future.
 In particular, someday we might make a 'SCANNING' option.
 SCANNING channels could have different frequencies and the radio would
 occasionally check that freq to see if anything is being transmitted.
 For devices that have multiple physical radios attached, we could keep multiple
 PRIMARY/SCANNING channels active at once to allow cross band routing as needed.
 If a device has only a single radio (the common case) only one channel can be
 PRIMARY at a time (but any number of SECONDARY channels can't be sent received
 on that common frequency) */
typedef enum _meshtastic_Channel_Role {
  /* This channel is not in use right now */
  meshtastic_Channel_Role_DISABLED = 0,
  /* This channel is used to set the frequency for the radio - all other enabled
     channels must be SECONDARY */
  meshtastic_Channel_Role_PRIMARY = 1,
  /* Secondary channels are only used for encryption/decryption/authentication
purposes. Their radio settings (freq etc) are ignored, only psk is used. */
  meshtastic_Channel_Role_SECONDARY = 2
} meshtastic_Channel_Role;

/* Struct definitions */
typedef PB_BYTES_ARRAY_T(32) meshtastic_ChannelSettings_psk_t;
/* Full settings (center freq, spread factor, pre-shared secret key etc...)
 needed to configure a radio for speaking on a particular channel This
 information can be encoded as a QRcode/url so that other users can configure
 their radio to join the same channel.
 A note about how channel names are shown to users: channelname-Xy
 poundsymbol is a prefix used to indicate this is a channel name (idea from
 @professr). Where X is a letter from A-Z (base 26) representing a hash of the
 PSK for this channel - so that if the user changes anything about the channel
 (which does force a new PSK) this letter will also change. Thus preventing user
 confusion if two friends try to type in a channel name of "BobsChan" and then
 can't talk because their PSKs will be different. The PSK is hashed into this
 letter by "0x41 + [xor all bytes of the psk ] modulo 26" This also allows the
 option of someday if people have the PSK off (zero), the users COULD type in a
 channel name and be able to talk. Y is a lower case letter from a-z that
 represents the channel 'speed' settings (for some future definition of speed)
 FIXME: Add description of multi-channel support and how primary vs secondary
 channels are used.
 FIXME: explain how apps use channels for security.
 explain how remote settings and remote gpio are managed as an example */
typedef struct _meshtastic_ChannelSettings {
  /* Deprecated in favor of LoraConfig.channel_num */
  uint32_t channel_num;
  /* A simple pre-shared key for now for crypto.
Must be either 0 bytes (no crypto), 16 bytes (AES128), or 32 bytes (AES256).
A special shorthand is used for 1 byte long psks.
These psks should be treated as only minimally secure,
because they are listed in this source code.
Those bytes are mapped using the following scheme:
`0` = No crypto
`1` = The special "default" channel key: {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29,
0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0xbf} `2` through 10 = The
default channel key, except with 1 through 9 added to the last byte. Shown to
user as simple1 through 10 */
  meshtastic_ChannelSettings_psk_t psk;
  /* A SHORT name that will be packed into the URL.
Less than 12 bytes.
Something for end users to call the channel
If this is the empty string it is assumed that this channel
is the special (minimally secure) "Default"channel.
In user interfaces it should be rendered as a local language translation of "X".
For channel_num hashing empty string will be treated as "X".
Where "X" is selected based on the English words listed above for ModemPreset */
  char name[12];
  /* Used to construct a globally unique channel ID.
The full globally unique ID will be: "name.id" where ID is shown as base36.
Assuming that the number of meshtastic users is below 20K (true for a long time)
the chance of this 64 bit random number colliding with anyone else is super low.
And the penalty for collision is low as well, it just means that anyone trying
to decrypt channel messages might need to try multiple candidate channels. Any
time a non wire compatible change is made to a channel, this field should be
regenerated. There are a small number of 'special' globally known (and fairly)
insecure standard channels. Those channels do not have a numeric id included in
the settings, but instead it is pulled from a table of well known IDs. (see Well
Known Channels FIXME) */
  uint32_t id;
  /* If true, messages on the mesh will be sent to the *public* internet by any
   * gateway ndoe */
  bool uplink_enabled;
  /* If true, messages seen on the internet will be forwarded to the local mesh.
   */
  bool downlink_enabled;
} meshtastic_ChannelSettings;

/* A pair of a channel number, mode and the (sharable) settings for that channel
 */
typedef struct _meshtastic_Channel {
  /* The index of this channel in the channel table (from 0 to
MAX_NUM_CHANNELS-1) (Someday - not currently implemented) An index of -1 could
be used to mean "set by name", in which case the target node will find and set
the channel by settings.name. */
  int8_t index;
  /* The new settings, or NULL to disable that channel */
  bool has_settings;
  meshtastic_ChannelSettings settings;
  /* TODO: REPLACE */
  meshtastic_Channel_Role role;
} meshtastic_Channel;

#ifdef __cplusplus
extern "C" {
#endif

/* Helper constants for enums */
#define _meshtastic_Channel_Role_MIN meshtastic_Channel_Role_DISABLED
#define _meshtastic_Channel_Role_MAX meshtastic_Channel_Role_SECONDARY
#define _meshtastic_Channel_Role_ARRAYSIZE                                     \
  ((meshtastic_Channel_Role)(meshtastic_Channel_Role_SECONDARY + 1))

#define meshtastic_Channel_role_ENUMTYPE meshtastic_Channel_Role

/* Initializer values for message structs */
#define meshtastic_ChannelSettings_init_default                                \
  { 0, {0, {0}}, "", 0, 0, 0 }
#define meshtastic_Channel_init_default                                        \
  {                                                                            \
    0, false, meshtastic_ChannelSettings_init_default,                         \
        _meshtastic_Channel_Role_MIN                                           \
  }
#define meshtastic_ChannelSettings_init_zero                                   \
  { 0, {0, {0}}, "", 0, 0, 0 }
#define meshtastic_Channel_init_zero                                           \
  {                                                                            \
    0, false, meshtastic_ChannelSettings_init_zero,                            \
        _meshtastic_Channel_Role_MIN                                           \
  }

/* Field tags (for use in manual encoding/decoding) */
#define meshtastic_ChannelSettings_channel_num_tag 1
#define meshtastic_ChannelSettings_psk_tag 2
#define meshtastic_ChannelSettings_name_tag 3
#define meshtastic_ChannelSettings_id_tag 4
#define meshtastic_ChannelSettings_uplink_enabled_tag 5
#define meshtastic_ChannelSettings_downlink_enabled_tag 6
#define meshtastic_Channel_index_tag 1
#define meshtastic_Channel_settings_tag 2
#define meshtastic_Channel_role_tag 3

/* Struct field encoding specification for nanopb */
#define meshtastic_ChannelSettings_FIELDLIST(X, a)                             \
  X(a, STATIC, SINGULAR, UINT32, channel_num, 1)                               \
  X(a, STATIC, SINGULAR, BYTES, psk, 2)                                        \
  X(a, STATIC, SINGULAR, STRING, name, 3)                                      \
  X(a, STATIC, SINGULAR, FIXED32, id, 4)                                       \
  X(a, STATIC, SINGULAR, BOOL, uplink_enabled, 5)                              \
  X(a, STATIC, SINGULAR, BOOL, downlink_enabled, 6)
#define meshtastic_ChannelSettings_CALLBACK NULL
#define meshtastic_ChannelSettings_DEFAULT NULL

#define meshtastic_Channel_FIELDLIST(X, a)                                     \
  X(a, STATIC, SINGULAR, INT32, index, 1)                                      \
  X(a, STATIC, OPTIONAL, MESSAGE, settings, 2)                                 \
  X(a, STATIC, SINGULAR, UENUM, role, 3)
#define meshtastic_Channel_CALLBACK NULL
#define meshtastic_Channel_DEFAULT NULL
#define meshtastic_Channel_settings_MSGTYPE meshtastic_ChannelSettings

extern const pb_msgdesc_t meshtastic_ChannelSettings_msg;
extern const pb_msgdesc_t meshtastic_Channel_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define meshtastic_ChannelSettings_fields &meshtastic_ChannelSettings_msg
#define meshtastic_Channel_fields &meshtastic_Channel_msg

/* Maximum encoded size of messages (where known) */
#define meshtastic_ChannelSettings_size 62
#define meshtastic_Channel_size 77

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
