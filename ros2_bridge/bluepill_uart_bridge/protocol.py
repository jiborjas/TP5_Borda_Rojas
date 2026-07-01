from dataclasses import dataclass
from enum import Enum


START_CHAR = "@"
END_CHAR = "\n"
SEPARATOR = ":"
TYPE_LENGTH = 3
MAX_PAYLOAD_LENGTH = 48


class MessageType(str, Enum):
    CMD = "CMD"
    DAT = "DAT"
    EVT = "EVT"
    STS = "STS"
    ACK = "ACK"
    ERR = "ERR"


@dataclass
class ProtocolMessage:
    msg_type: MessageType
    payload: str


def compute_checksum(data: str) -> int:
    raise NotImplementedError("Alumno: implementar checksum XOR byte a byte")


def encode_frame(message: ProtocolMessage) -> str:
    raise NotImplementedError("Alumno: implementar @LL:TTT:PAYLOAD:CC\\n")


def decode_body(body: str) -> ProtocolMessage:
    raise NotImplementedError("Alumno: implementar decode de TTT:PAYLOAD")


class ParseResult(Enum):
    IN_PROGRESS = 0
    MESSAGE_READY = 1
    ERROR = 2


class IncrementalParser:
    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.state = "WAIT_START"
        self.length_field = ""
        self.body = ""
        self.checksum_field = ""
        self.expected_length = 0

    def consume(self, byte: str):
        raise NotImplementedError(
            "Alumno: implementar FSM incremental del parser UART"
        )
