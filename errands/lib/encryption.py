# Copyright 2020 gdavid7 https://github.com/gdavid7/cryptocode
# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from base64 import b64encode, b64decode
import hashlib
from Cryptodome.Cipher import AES
from Cryptodome.Random import get_random_bytes


def encrypt(message: str, password: str) -> str:
    """Encrypt string with password"""

    # generate a random salt
    salt: bytes = get_random_bytes(AES.block_size)

    # use the Scrypt KDF to get a private key from the password
    private_key: bytes = hashlib.scrypt(
        password.encode(),
        salt=salt,
        n=2**14,
        r=8,
        p=1,
        dklen=32,
    )

    # create cipher config
    cipher_config = AES.new(private_key, AES.MODE_GCM)

    # return a dictionary with the encrypted text
    (cipher_text, tag) = cipher_config.encrypt_and_digest(bytes(message, "utf-8"))
    encryptedDict: dict[str, str] = {
        "cipher_text": b64encode(cipher_text).decode("utf-8"),
        "salt": b64encode(salt).decode("utf-8"),
        "nonce": b64encode(cipher_config.nonce).decode("utf-8"),
        "tag": b64encode(tag).decode("utf-8"),
    }
    encryptedString: str = (
        encryptedDict["cipher_text"]
        + "*"
        + encryptedDict["salt"]
        + "*"
        + encryptedDict["nonce"]
        + "*"
        + encryptedDict["tag"]
    )
    return encryptedString


def decrypt(enc_message: str, password: str) -> str | None:
    """Decrypt string with password"""

    enc_dict: list[str] = enc_message.split("*")
    try:
        enc_dict: dict[str, str] = {
            "cipher_text": enc_dict[0],
            "salt": enc_dict[1],
            "nonce": enc_dict[2],
            "tag": enc_dict[3],
        }

        # decode the dictionary entries from base64
        salt: bytes = b64decode(enc_dict["salt"])
        cipher_text: bytes = b64decode(enc_dict["cipher_text"])
        nonce: bytes = b64decode(enc_dict["nonce"])
        tag: bytes = b64decode(enc_dict["tag"])

        # generate the private key from the password and salt
        private_key: bytes = hashlib.scrypt(
            password.encode(),
            salt=salt,
            n=2**14,
            r=8,
            p=1,
            dklen=32,
        )

        # create the cipher config
        cipher = AES.new(private_key, AES.MODE_GCM, nonce=nonce)

        # decrypt the cipher text
        decrypted: bytes = cipher.decrypt_and_verify(cipher_text, tag)
    except:
        return None

    return decrypted.decode("UTF-8")
