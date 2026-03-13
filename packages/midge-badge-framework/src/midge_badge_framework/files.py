from ctypes import Structure, c_int8, c_uint8, c_uint64

from .protocol import CustomAdvertisementData


class ScanFileEntry(Structure):
    _fields_ = [
        ("rssi", c_int8),
        ("mac_address", c_uint8 * 6),
        ("advertised_data", CustomAdvertisementData),
        ("timestamp", c_uint64),
    ]

    def __str__(self):
        ret = f"{self.__class__.__name__} : "
        for field in self._fields_:
            attr = field[0]
            if attr == "mac_address":
                ret += "[mac_addr = "
                arr = getattr(self, attr)
                for x in arr:
                    ret += f"{x:02x}:"
                ret = ret[0:-1]
                ret += "]"
            else:
                ret += f"[{attr} = {getattr(self, attr)}]"
        return ret
