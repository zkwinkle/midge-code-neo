from abc import abstractmethod
from ctypes import Structure, Union, c_int16, c_int32, c_uint8, c_uint16, c_uint64


def mingle_midge_batt_mv_to_percent(mv):
    v_percentage_vector = [
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        3,
        3,
        3,
        3,
        3,
        3,
        3,
        4,
        4,
        4,
        4,
        4,
        4,
        5,
        5,
        5,
        6,
        6,
        7,
        7,
        8,
        8,
        9,
        9,
        10,
        11,
        12,
        13,
        13,
        14,
        15,
        16,
        18,
        19,
        22,
        25,
        28,
        32,
        36,
        40,
        44,
        47,
        51,
        53,
        56,
        58,
        60,
        62,
        64,
        66,
        67,
        69,
        71,
        72,
        74,
        76,
        77,
        79,
        81,
        82,
        84,
        85,
        85,
        86,
        86,
        86,
        87,
        88,
        88,
        89,
        90,
        91,
        91,
        92,
        93,
        94,
        95,
        96,
        97,
        98,
        99,
        100,
        100,
    ]
    batt_meas_low_mv = 3000
    # batt_meas_full_mv = 4200
    batt_meas_mv_to_soc_delta = 11
    batt_meas_mv_to_soc_elements = 111
    vector_index = int((mv - batt_meas_low_mv) / batt_meas_mv_to_soc_delta)
    if vector_index < 0:
        vector_index = 0
    elif vector_index > (batt_meas_mv_to_soc_elements - 1):
        vector_index = batt_meas_mv_to_soc_elements - 1

    return v_percentage_vector[vector_index]


def get_bitfield_width(struct_cls, name):
    for fname, _, *rest in struct_cls._fields_:
        if fname == name:
            # rest will contain the width if it s a bit-field
            return rest[0] if rest else None
    raise KeyError(name)


class MidgeBadgeCommand(Structure):
    @abstractmethod
    def id() -> bytes:
        pass

    def __str__(self):
        ret = f"{self.__class__.__name__} : "
        for field in self._fields_:
            if field[0] == "battery_millivolts":
                mv = int(getattr(self, field[0]))
                # future proof: Based on the midge badge version, select a
                # correct mv to battery percentage function
                batt_percent = mingle_midge_batt_mv_to_percent(mv)
                ret += f"[battery = {batt_percent} %]"
            else:
                ret += f"[{field[0]} = {getattr(self, field[0])}]"
        return ret


class BadgeID(Structure):
    _fields_ = [
        ("group", c_uint16, 4),  # 4-bit field
        ("badge", c_uint16, 12),  # 12-bit field
    ]

    def __str__(self):
        ret = f"{self.__class__.__name__} : "
        for field in self._fields_:
            ret += f"[{field[0]} = {getattr(self, field[0])}]"
        return ret


class BadgeAssignment(Union):
    _fields_ = [
        ("id", BadgeID),
        ("u16_all", c_uint16),
    ]

    def __str__(self):
        ret = f"{self.__class__.__name__} : "
        for field in self._fields_:
            ret += f"[{field[0]} = {getattr(self, field[0])}]"
        return ret


class CmdSetupExperimentRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("badge_assignment", BadgeAssignment),
        ("experiment_id", c_uint16),
    ]

    def id() -> bytes:
        return b"A"


class CmdSetupExperimentResponse(Structure):
    _pack_ = 1
    _fields_ = [("status_code", c_uint8)]

    def id() -> bytes:
        return b"A"


class CmdStatusRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("millis_since_epoch", c_uint64)]

    def id() -> bytes:
        return b"B"


class CmdStatusResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [
        ("sync_status", c_uint8),
        ("storage_init_status", c_uint8),
        ("audio_init_status", c_uint8),
        ("proximity_init_status", c_uint8),
        ("battery_millivolts", c_int16),
        ("badge_assigment", BadgeAssignment),
        ("delta_ms", c_uint64),
    ]

    def id() -> bytes:
        return b"B"


class CmdGetFWVersionRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("reserved", c_uint16)]

    def id() -> bytes:
        return b"C"


class CmdGetFWVersionResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("version_str", c_uint8 * 60)]

    def id() -> bytes:
        return b"C"


class CmdStartMicRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("sample_id", c_uint16), ("mode", c_uint8)]

    def id() -> bytes:
        return b"D"


class CmdStartMicResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("status_code", c_int32)]

    def id() -> bytes:
        return b"D"


class CmdStopMicRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("reserved", c_uint16)]

    def id() -> bytes:
        return b"E"


class CmdStopMicResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("reserved", c_int32)]

    def id() -> bytes:
        return b"E"


class CmdStartScanRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("sample_id", c_uint16), ("window", c_uint16), ("interval", c_uint16)]

    def id() -> bytes:
        return b"F"


class CmdStartScanResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("status_code", c_int32)]

    def id() -> bytes:
        return b"F"


class CmdStopScanRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("reserved", c_uint16)]

    def id() -> bytes:
        return b"G"


class CmdStopScanResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("status_code", c_int32)]

    def id() -> bytes:
        return b"G"


class CmdEraseSDRequest(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("reserved", c_uint16)]

    def id() -> bytes:
        return b"H"


class CmdEraseSDResponse(MidgeBadgeCommand):
    _pack_ = 1
    _fields_ = [("status_code", c_int32)]

    def id() -> bytes:
        return b"H"


# DATA
CMD_RESPONSES: list[MidgeBadgeCommand] = [
    CmdSetupExperimentResponse,
    CmdStatusResponse,
    CmdGetFWVersionResponse,
    CmdStartMicResponse,
    CmdStopMicResponse,
    CmdStartScanResponse,
    CmdStopScanResponse,
    CmdEraseSDResponse,
]

SOT = b"#"
EOT = b"!"
