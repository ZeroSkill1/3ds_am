from Cryptodome.Hash import CMAC, SHA256
from pyctr.crypto import CryptoEngine
from Cryptodome.Cipher.AES import *
from typing import BinaryIO, Tuple
from Cryptodome.Cipher import AES
from enum import IntEnum
from math import ceil
import struct
import sys
import os

BANNER_SIZE = 0x4000
DECRYPT_KEY = None
CMAC_KEY = None

class ExportSection:
	data: bytes
	mac: bytes
	iv: bytes
	decrypted_sha256: bytes = None
	
	def __init__(self, section, mac, iv) -> None:
		self.data = section
		self.mac = mac
		self.iv = iv

class ExportHeader:
	raw_data: bytes = None

	magic: str = None
	group_id: int = None
	title_version: int = None
	encrypted_movable_hash: bytes = None
	zeroiv_crypted_zeros_cbc_block: bytes = None
	title_id: int = None
	required_size: int = None
	payload_sizes: list[int] = None
	private_save_size: int = None
	

	def __init__(self, data: bytes, section_count: int):
		self.magic                          = data[0x0:0x4].decode("ascii")
		self.group_id                       = int.from_bytes(data[0x4:0x6], "little")
		self.title_version                  = int.from_bytes(data[0x6:0x8], "little")
		self.encrypted_movable_hash         = data[0x8:0x28]
		self.zeroiv_crypted_zeros_cbc_block = data[0x28:0x38]
		self.title_id                       = int.from_bytes(data[0x38:0x40], "little")
		self.required_size                  = int.from_bytes(data[0x40:0x48], "little")
		
		payload_and_more = data[0x48:]
		unpack_format = "<" + "I" * section_count
		self.payload_sizes = list(struct.unpack(unpack_format, payload_and_more[0:section_count * 4]))
		if section_count == 12:
			self.private_save_size = int.from_bytes(data[0xF0:0xF4], "little")

	def __str__(self) -> str:
		return \
			f"Magic                              : {self.magic}\n" \
			f"Group ID                           : {self.group_id:04X}\n" \
			f"Title Version                      : {self.title_version}\n" \
			f"Encrypted movable.sed SHA-256 hash : {self.encrypted_movable_hash.hex().upper()}\n" \
			f"Zero-IV crypted CBC-block          : {self.zeroiv_crypted_zeros_cbc_block.hex().upper()}\n" \
			f"Title ID                           : {self.title_id:016X}\n" \
			f"Required Size:                     : {self.required_size} ({ceil(self.required_size / 1024 / 128):.0F} blocks)\n" \
			f"Private save size                  : {self.private_save_size if self.private_save_size else 'N/A'}\n"

class TWLExportType(IntEnum):
	# 12 content sections
	V2_12ContentSections7 = 0x7
	V2_12ContentSections8 = 0x8
	V2_12ContentSections9 = 0x9
	V2_12ContentSectionsA = 0xA
	V2_12ContentSectionsB = 0xB
	# 4 content sections
	V2_4ContentSectionsC  = 0xC
	V1_4ContentSectionsD  = 0xD
	# 11 content sections
	V2_11ContentSectionsE = 0xE
	Default = 0xFF # this will be used if the export type is not one of the previous ones, also uses 11 content sections

def get_export_info(export_type: int) -> Tuple[int, int, int]:
	if 7 <= export_type <= 0xB:
		return (0x100, 0x500, 12)
	elif export_type == 0xC or export_type == 0xD:
		return (0xA0, 0x400, 4)
	else:
		return (0xF0, 0x4E0, 11)

def align(x: int, y: int) -> int:
	return 0 if x == 0 else ((x + y - 1) & ~(y - 1))

def read_section(fp: BinaryIO, size: int) -> ExportSection:
	sect = fp.read(align(size, 0x10))
	mac = fp.read(0x10)
	iv = fp.read(0x10)
	return ExportSection(sect, mac, iv)

def decrypt_and_verify_section(section: ExportSection) -> bytes:
	cipher = AES.new(DECRYPT_KEY, MODE_CBC, section.iv)
	decrypted = cipher.decrypt(section.data)

	sha = SHA256.new()
	sha.update(decrypted)
	sha256_digest = sha.digest()

	cmac = CMAC.new(CMAC_KEY, ciphermod=AES)
	cmac.update(sha256_digest)
	cmac_digest = cmac.digest()

	if cmac_digest != section.mac:
		raise Exception(f"CMAC invalid! Expected {section.mac.hex()} but got {cmac_digest.hex()}")

	section.decrypted_sha256 = sha256_digest

	return decrypted

def decrypt_verify_section_to_file(export_fp: BinaryIO, path: str, size: int, name: str):
	print(f"/================== {name} ==================\\")
	print(f"  Offset: 0x{export_fp.tell():X}")
	print(f"  Size: 0x{size:X} ({size})")
	section = read_section(export_fp, size)
	print(f"  CMAC: {section.mac.hex().upper()}")
	print(f"  IV: {section.iv.hex().upper()}")
	section_decrypted = decrypt_and_verify_section(section)
	print(f"\\================== {name} ==================/\n")

	with open(path, "wb") as f:
		f.write(section_decrypted)

	return section_decrypted

def parse_export(path: str, movable_path: str, export_type_int: int, allow_default_export_fallback: bool = False):
	if export_type_int < 0 or export_type_int > 0xFF:
		raise Exception("Export types must be within 0x00-0xFF.")

	must_use_default = export_type_int < 7 or export_type_int > 0xD

	if must_use_default and not allow_default_export_fallback:
		raise Exception(f"Invalid export type {export_type_int:02X}, cannot fall back to default")

	if must_use_default:
		export_type = TWLExportType.Default
	else:
		for type in TWLExportType:
			if type.value == export_type_int:
				export_type = type
				break

	info = get_export_info(export_type.value)
	header_size    = info[0]
	footer_size    = info[1]
	sections_count = info[2]

	if not os.path.isfile(path):
		raise FileNotFoundError(path)

	if os.path.getsize(path) < header_size + 0x20 + footer_size + 0x20 + BANNER_SIZE:
		raise Exception("Input file is too small.")

	with open(movable_path, "rb") as f:
		f.seek(0x110)
		keyy = f.read(0x10)

	crypt = CryptoEngine()
	
	crypt.set_keyslot("y", 0x34, keyy) # crypt key
	crypt.set_keyslot("y", 0x3A, keyy) # cmac key

	global DECRYPT_KEY
	global CMAC_KEY

	DECRYPT_KEY = crypt.key_normal[0x34]
	CMAC_KEY = crypt.key_normal[0x3A]

	with open(path, "rb") as export_fp:
		decrypt_verify_section_to_file(export_fp, "banner.bin", BANNER_SIZE, "banner")
		header = decrypt_verify_section_to_file(export_fp, "header.bin", header_size, "header")
		decrypt_verify_section_to_file(export_fp, "footer.bin", footer_size, "footer")
		hdr = ExportHeader(header, sections_count)
		for idx, payload_size in enumerate(hdr.payload_sizes):
			if payload_size == 0:
				continue

			decrypt_verify_section_to_file(export_fp, f"contentsection_{idx}.bin", payload_size, f"Content section {idx}")
		if hdr.private_save_size != None and hdr.private_save_size != 0:
			decrypt_verify_section_to_file(export_fp, f"contentsection_privatesav.bin", hdr.private_save_size, "Private save data")

		print(hdr)
		print(f"Resolved export type               : {export_type.name} ({export_type.value})")
		print(f"Header size                        : 0x{header_size:X}")
		print(f"Footer size                        : 0x{footer_size:X}")
		print(f"Section count                      : {sections_count}")
try:
	rawint = int(sys.argv[3], base=16)
except Exception as e:
	sys.exit("Invalid integer")

parse_export(sys.argv[1], sys.argv[2], rawint, allow_default_export_fallback=True)