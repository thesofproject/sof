#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation. All rights reserved.
#
# The usage of this command is simple. It takes one argument, a path
# to a SOF topology2 source file, and it produces to stdout something
# that can be parsed by Doxygen as C source. Produced output contains
# C struct definitions and instantiations analogous to the topology2
# classes and object instances.
#
# The purpose of the translated C code is not to document actual
# topology2 code, but only to provide anchors for Doxygen to form a
# network of links through which to navigate the topology sources and
# find the pieces of related Doxygen documentation. The filter also
# creates separate pages of the original code and adds links next to the
# pages in the C struct definition, instance documentation and their
# possible Doxygen documentation.

import sys
import os
import re
import io
import logging

logging.basicConfig(filename='filter_debug.txt', filemode='a', encoding='utf-8',
		    level=logging.DEBUG)

def fname():
	try:
		name = sys._getframe(1).f_code.co_name
	except (IndexError, TypeError, AttributeError):
		name = "<unknown>"
	return name

def cbracket_count(line):
	val = line.count("{") - line.count("}")
	return val

def sbracket_count(line):
	val = line.count("[") - line.count("]")
	return val

def doxy_check_add(doxy, line):
	if line.find("##") >= 0:
		doxy = doxy + line[line.find("##"):].replace("##", "//!", 1)
	if line.find("#") >= 0:
		line = line[0:line.find("#")]
	return (doxy, line)

def print_doxy(doxy, file = sys.stdout):
	if len(doxy):
		print(doxy, file=file)
		return ""
	return doxy

def parse_include_str(line):
	if re.search(r"^\s*\<[A-Za-z0-9\/_\-\.]+\>\s*", line):
		tok = line.split()
		return "#include " + tok[0] + "\n"
	return None

def parse_include(line):
	str = parse_include_str(line)
	if str:
		print(str)
		return True
	return False

def parse_define_block(fline, instream):
	"""Parses topology2 Define { } block and outputs C-preprocessor #define macros

	Parameters:
	fline (string): First input line that was read by the caller
	instream (stream): Input stream of topology2 file we are decoding

	Returns:
	string: The original code that was translated
	"""
	if re.search(r"^\s*Define\s+\{", fline):
		logging.debug("fline: %s", fline)
		doxy = ""
		for line in instream:
			if cbracket_count(line) < 0:
				break
			(doxy, line) = doxy_check_add(doxy, line)
			doxy = print_doxy(doxy)
			tok = line.split(maxsplit = 2)
			if len(tok) < 2:
				continue
			val = trim_value(line[line.find(tok[0]) + len(tok[0]):])
			print("#define %s\t%s" % (trim_c_id(tok[0]), val))
		return True
	return False

def parse_include_by_key(fline, instream, file): # For now just skip
	"""Handles IncludeByKey { } blocks, currently handles only actual
	   includes, not nested {} blocks but everything is included into
           raw_code return value.

	Parameters:
	fline (string): First input line that was read by the caller
	instream (stream): Input stream of topology2 file we are decoding
	file (stream): Output stream for the translated output

	Returns:
	string: The original code that was translated

	"""
	if re.search(r"^\s*IncludeByKey\.", fline):
		logging.debug("fline: %s", fline)
		bsum = cbracket_count(fline)
		if bsum == 1:
			# Assume IncludeByKey.<variable> {
			tok = fline.split()
			tok = tok[0].split(".")
			name = tok[1]
			raw_code = ""
			ifstr = "if"
			for line in instream:
				raw_code = raw_code + line
				bsum = bsum + cbracket_count(line)
				tok = line.split("\"")
				if len(tok) >= 4:
					print("#%s %s == \"%s\"\n#include <%s>" %
					      (ifstr, name, tok[1], tok[3]), file = file)
					if ifstr == "if":
						ifstr = "elif"
				if bsum < 1:
					if ifstr != "if":
						print("#endif", file = file)
					return raw_code
	return None

def trim_value(val):
	val = val.strip(" \t\n$")
	end = len(val) - 1
	if val[0:1] == "\"" and val[end:] == "\"":
		return val
	if val.isidentifier() or val.isnumeric():
		return val
	if val[0:1] == "\'" and val[end:] == "\'":
		val = val.strip("\'")
	return "\"" + val + "\""

def trim_c_id(name):
	name = name.strip(" \t\n\"\'")
	name = name.replace("-", "_")
	name = name.replace(" ", "_")
	return name


def parse_attribute_constraints(instream, name):
	"""Parses a Constraints { } block and produces a C enum definition if possible

	Parameters:
	instream (stream): Input stream of topology2 file we are decoding
	name (string): Attribute name of this constraints block belongs to

	Returns:
	string: C enum definition or an empty string

	"""
	logging.debug("name: %s", name)
	valid_values = []
	tuple_values = []
	doxy = "" # This is thrown away since there is no C anchror to connect it-
	raw_code = ""
	enum  = ""
	for line in instream:
		raw_code = raw_code + line
		(doxy, line) = doxy_check_add(doxy, line)
		if cbracket_count(line) < 0:
			break
		if re.search(r"^\s*\!valid_values\s+\[", line):
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				valid_values.append(trim_c_id(line))
		if re.search(r"^\s*\!tuple_values\s+\[", line):
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				tuple_values.append(trim_value(line))
	if len(valid_values) > 1 and len(valid_values) == len(tuple_values):
		enum = "enum " + name + " {\n"
		for i in range(len(valid_values)):
		       enum = enum + "\t\t" + valid_values[i] + " =\t" + tuple_values[i] + ",\n"
		enum = enum + "\t}"
	elif len(valid_values) > 1 and len(tuple_values) == 0:
		enum = "enum " + name + " {\n"
		for i in range(len(valid_values)):
		       enum = enum + "\t\t" + valid_values[i] + ","
		       if valid_values[i][0:1] == "$": # If its a variable add a reference to it
			       enum = enum + " //!< \\ref " + valid_values[i][1:]
		       enum = enum + "\n"
		enum = enum + "\t}"
	return (raw_code, enum)

def parse_class_attribute(attributes, doxy, instream, fline):
	"""Parses a DefineAttribute { } block and collects the information into
	   attributes dict. Any already accumulated Doxygen documentation is
	   also stored there under the attribute name-

	Parameters:
	attributes (dict): A dictionary where data about the attribute is stored
	doxy (strung): Doxygen documentation collected just before the attribute
	instream (stream): Input stream of topology2 file we are decoding
	fline (string): First input line that was read by the caller

	Returns:
	string: The original code that was translated
	"""
	logging.debug("fline: %s", fline)
	tok = fline.split("\"")
	if len(tok) > 1:
		name = tok[1]
	else:
		tok = fline.split(".")
		tok = tok[1].split(" ")
		name = tok[0]
	(doxy, fline) = doxy_check_add(doxy, fline)
	bsum = cbracket_count(fline)
	typestr = ""
	token_ref = ""
	ref_type = ""
	enum = ""
	raw_code = ""
	if bsum < 1:
		# Assume: DefineAttribute.name {}
		typestr = "int"
	else:
		for line in instream:
			raw_code = raw_code + line
			(doxy, line) = doxy_check_add(doxy, line)
			bsum = bsum + cbracket_count(line)
			if bsum < 1:
				break
			if re.search(r"^\s*constraints\s+\{", line) and bsum == 2:
				(code, enum) = parse_attribute_constraints(instream, name)
				raw_code = raw_code + code
				bsum = 1
			elif re.search(r"^\s*type\s", line):
				tok = line.split()
				typestr = tok[1].strip(" \t\n\"\'")
			elif re.search(r"^\s*token_ref\s", line):
				tok = line.split()
				token_ref = tok[1].strip(" \t\n\"\'")
				tok = token_ref.split(".")
				ref_type = tok[1]
	logging.debug("type %s token_ref %s ref_type %s enum %s",
		      typestr, token_ref, ref_type, enum)
	if enum != "" and ref_type != "bool":
		typestr = enum
		if ref_type == "string":
			doxy = doxy + "//! \\em string type\n"
	elif typestr == "" or ref_type == "bool":
		typestr = ref_type
	attributes[name] = { "type": typestr, "doxy": doxy, "token_ref": token_ref }
	return raw_code

def print_attributes(attributes):
	"""Print out struct members and their doxygen documentation from attributes dict.

	Parameters:
	attributes (dict): A dictionary where data about the attributes was stored
	"""
	for name in attributes:
		doxy = ""
		if attributes[name].get("doxy"):
			doxy = attributes[name]["doxy"] + "\n"
		if attributes[name].get("type"):
			typestr = attributes[name]["type"]
			print("%s\t%s %s;\n" % (doxy, typestr, name))

def set_attribute_flag(attributes, attribute, flag):
	if not attributes.get(attribute):
		attributes[attribute] = {}
	attributes[attribute][flag] = True

def parse_attributes_block(instream, attributes, attrib_doxy):
	"""Parse attributes block inside class definition and store the flags in attributes dict

	Parameters:
	instream (stream): Input stream of topology2 file we are decoding
	attributes (dict): Dict where data about the attributes is stored
	attrib_doxy (dict): Dict to store doxygen docs associated with the attributes block

	Returns:
	string: The original code that was translated
	"""
	logging.debug("called")
	raw_code = ""
	doxy = ""
	for line in instream:
		raw_code = raw_code + line
		(doxy, line) = doxy_check_add(doxy, line)
		if cbracket_count(line) < 0:
			break
		if re.search(r"^\s*\!constructor\s+\[", line):
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				set_attribute_flag(attributes, trim_c_id(line), "constructor")
			attrib_doxy["constructor"] = doxy
			doxy = ""
		elif re.search(r"^\s*\!mandatory\s+\[", line):
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				set_attribute_flag(attributes, trim_c_id(line), "mandatory")
			attrib_doxy["mandatory"] = doxy
			doxy = ""
		elif re.search(r"^\s*\!immutable\s+\[", line):
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				set_attribute_flag(attributes, trim_c_id(line), "immutable")
			attrib_doxy["immutable"] = doxy
			doxy = ""
		elif re.search(r"^\s*\!deprecated\s+\[", line):
			attrib_doxy["deprecated"] = doxy
			doxy = ""
			for line in instream:
				raw_code = raw_code + line
				(doxy, line) = doxy_check_add(doxy, line)
				if sbracket_count(line) < 0:
					break
				set_attribute_flag(attributes, trim_c_id(line), "deprecated")
		elif re.search(r"^\s*unique\s+", line):
			tok = line.split()
			attrib_doxy["unique"] = doxy
			doxy = ""
			set_attribute_flag(attributes, trim_c_id(tok[1]), "unique")
	return raw_code

def attribute_block_print(class_name, attributes, attrib_doxy):
	"""Generates a Doxygen documentation section from attribute flags
	   and doxygen docs stored to attrib_doxy

	Parameters:
	class_name (string): Name of the class we are processing
	attributes (dict): Dict where data about the attributes was stored
	attrib_doxy (dict): Dict where doxygen docs of the attributes block was stored

	"""
	logging.debug("class_name \'%s\'", class_name)
	for field in attrib_doxy:
		# Only add attribute paragraph if there are doxygen comment for it
		if attrib_doxy[field] != "":
			print("//! \\par %s attributes:\n//!" % field.capitalize())
			for attr in attributes:
				if attributes[attr].get(field):
					print("//! \\link %s::%s \\endlink \\n" % (class_name, attr))
			print("//! \\n\n%s" % attrib_doxy[field])

def attribute_block_info_add(attributes, attrib_doxy):
	"""Add simple bullets in the attribute (= struct members) documentation if
	   the attribute has constructor, mandatory, immutable, deprecated, or unique
	   property.

	Parameters:
	attributes (dict): Dict where data about the attributes was stored
	attrib_doxy (dict): Dict where doxygen docs of the attributes block was stored

	"""
	# TODO: Add short documentation about the attribute properties and link to it.
	for field in attrib_doxy:
		for attr in attributes:
			if attributes[attr].get(field):
				if not attributes[attr].get("doxy"):
					attributes[attr]["doxy"] = ""
				attributes[attr]["doxy"] = "//! - \\em " + field + " attribute.\\n\n" + attributes[attr]["doxy"]

def parse_class_contents(class_name, attributes, attrib_doxy, objs, defaults,
			 instream, includef = ""):
	"""Translates the contents inside a class definition { } block

	Parameters:
	class_name (string): Class name
	attributes (dict): Dict where data about the attributes is stored
	attrib_doxy (dict): Dict to store doxygen docs associated with the attributes block
	objs (dict): Dict to store fist level contained objects with Docxyge docs
	defaults (streaM): Stream where the instances of the objects are printed
	instream (stream): Input stream of topology2 file we are decoding
	includef (string): Include file from where the attributes and objects inline included from

	Returns:
	string: The original code that was translated
	"""
	bsum = 1
	doxy_addition = ""
	if includef != "":
		doxy_addition = "//! - Included from <" + includef + ">\\n\n"
	doxy = ""
	raw_code = ""
	for line in instream:
		raw_code = raw_code + line
		if parse_include_str(line):
			# Inline files that are included from within a class definition
			filename = line[line.find("<") + 1:line.find(">")]
			logging.debug("try to inline include \'%s\' from \'%s\'",
				      filename, os.getcwd())
			# NOTE: The path is relative to when the script exists so
			#       two levels up and we are at topology2 root
			with open("../../" + filename, "r+", encoding="ascii") as ifile:
				parse_class_contents(class_name, attributes, attrib_doxy, objs,
						     defaults, ifile, filename)
			continue
		if (code := parse_include_by_key(line, instream, sys.stdout)):
			raw_code = raw_code + code
			continue
		if re.search(r"^\s*DefineAttribute\.", line):
			doxy = doxy_addition + doxy
			raw_code = raw_code + parse_class_attribute(attributes, doxy, instream, line)
			doxy = ""
		elif re.search(r"^\s*DefineArgument\.", line):
			for line in instream: # Just skip, this is only used in bytes.conf
				if cbracket_count(line) < 0:
					break
		elif re.search(r"^\s*attributes\s+\{", line):
			doxy = "" # Doxy comments before attributes block end in weird places
			raw_code = raw_code + parse_attributes_block(instream, attributes, attrib_doxy)
		elif re.search(r"^\s*Object\.", line):
			# TODO: Pass collected doxy comments to parse_object and store in objs
			doxy = ""
			raw_code = raw_code + parse_object(instream, line, file = defaults,
							   objects = objs, tabs = "\t", ending = ",")
		else: # If nothing else matched, assume a default value definition for an attribute
			(doxy, line) = doxy_check_add(doxy, line)
			tok = line.split()
			if len(tok) == 2 and tok[0].isidentifier():
				if doxy != "":
					print(doxy, file = defaults)
					doxy = ""
				val = trim_value(line[line.find(tok[0]) + len(tok[0]):])
				print("\t.%s =\t%s," % (trim_c_id(tok[0]), val),
				      file = defaults)
			bsum = bsum + cbracket_count(line)
			if bsum < 1:
				break
	return raw_code

def parse_class(instream, fline):
	"""Parse class definition and print out C struct definition

	Parameters:
	fline (string): First input line that was read by the caller
	instream (stream): Input stream of topology2 file we are decoding
	"""
	logging.debug("fline: %s", fline)
	tok = fline.split(maxsplit = 2)
	cdef = tok[0].split(".")
	# base = cdef[1] # Just in case we go back to C++ tralation
	class_name = cdef[2]
	class_name = trim_c_id(class_name)
	attributes = {}
	attrib_doxy = {}
	defaults = io.StringIO()
	objs = {}
	raw_code = fline + parse_class_contents(class_name, attributes, attrib_doxy,
						objs, defaults, instream)
	attribute_block_print(class_name, attributes, attrib_doxy)
	attribute_block_info_add(attributes, attrib_doxy)
	print("//! \\ref %s_rawcode" % class_name)
	print("struct %s {" % class_name)
	print_attributes(attributes)
	for obj in objs:
		if objs[obj]["count"] > 1:
			for i in range(objs[obj]["count"]):
				print(objs[obj]["doxy"][i])
				print("\tstruct %s %s%d;" % (obj, obj, i))
		else:
			print(objs[obj]["doxy"][0])
			print("\tstruct %s %s;" % (obj, obj))
	print("};\n")
	print("/*! \\page %s_rawcode The %s class definition in topology2 code\n\t\\code{.unparsed}" %
	      (class_name, class_name))
	print(raw_code)
	print("\t\\endcode\n*/")
	print("//! \\var struct %s %s_defaults" % (class_name, class_name))
	print("//! \\brief %s class default values" % class_name)
	print("struct %s %s_defaults = {\n" % (class_name, class_name))
	print(defaults.getvalue())
	print("};\n")
	defaults.close()

def parse_members(instream, cbsum, tabs, file):
	"""Parse attribute initializations from object instantiation

	Parameters:
	instream (stream): Input stream of topology2 file we are decoding
	cbsum (int): The amount of curly brackets "{" WE HAVE OPEN
	tabs (string): Current level of indentation

	Returns:
	string: The original code that was translated
	"""
	doxy = ""
	raw_code = ""
	for line in instream:
		raw_code = raw_code + line
		if (code := parse_include_by_key(line, instream, file)):
			raw_code = raw_code + code
			continue
		cbsum = cbsum + cbracket_count(line)
		(doxy, line) = doxy_check_add(doxy, line)
		# Assume ending } to be alone on its own line
		if cbsum < 1:
			break
		if re.search(r"^\s*Object\.", line):
			logging.debug("object-line: %s", line)
			obj = line.split(".")
			if cbsum == 2:
				# Assume Object.Base.name.1 {
				doxy = print_doxy(doxy, file = file)
				raw_code = raw_code + parse_object(instream, line, file = file, tabs = tabs, ending = ",")
				cbsum = 1
			elif cbsum == 1:
				# Assume  Object.Base.name.1 {}
				doxy = print_doxy(doxy, file = file)
				print("%s.%s = {}," % (tabs, obj[2]), file = file)
		else:
			tok = line.split(maxsplit = 1)
			if len(tok) >=2:
				name = tok[0]
				val = trim_value(tok[1])
				doxy = print_doxy(doxy, file = file)
				print("%s.%s = %s," % (tabs, name, val), file = file)
	return raw_code

#
def object_instance_prefix(name, ending):
	"""Decide "struct name name =" or ".name =" based on instantiation ending in ',' or ';'

	Parameters:
	name (string): Name of the object instance
	ending (string): Either ',' or ';' indication if this is an instance or a definition

	"""
	if ending == ";":
		return "struct " + name + " "
	return "."

def parse_object(instream, fline, file = sys.stdout, objects = {}, tabs = "", ending = ";"):
	"""Translates all Object instatiation into initialized C structs
	   Note that dict arguments in python are passed as reference

	Parameters:
	instream (stream): Input stream of topology2 file we are decoding
	fline (string): First input line that was read by the caller
	file (stream): Where the C struct instance or definition is printed
	objects (dict): Dict to store fist level contained objects with Docxyge docs
	ending (string): Either ',' or ';' indication if this is an instance or a definition
	tabs (string): Current level of indentation

	Returns:
	string: The original code that was translated
	"""
	logging.debug("fline: %s", fline)
	tok = fline.split(maxsplit = 2)
	obj = tok[0].split(".")
	cbsum = cbracket_count(fline)
	sbsum = sbracket_count(fline)
	name = ""
	doxy = ""
	raw_code = ""
	if len(obj) == 4 and sbsum == 0 and cbsum == 0:
		# Assume Object.Base.name.1 { }
		name = obj[2]
		name = trim_c_id(name)
		doxy = print_doxy(doxy, file = file)
		prefix = object_instance_prefix(name, ending)
		print("%s%s = {}%s" % (prefix, name, ending), file = file)
		objects[name] = { "count": 1, "doxy": [doxy] }
	elif len(obj) == 4 and sbsum == 0 and cbsum == 1:
		# Assume Object.Base.name.1 {
		name = obj[2]
		name = trim_c_id(name)
		doxy = print_doxy(doxy, file = file)
		prefix = object_instance_prefix(name, ending)
		print("%s%s%s = {" % (tabs, prefix, name), file = file)
		raw_code = raw_code + parse_members(instream, cbsum, tabs + "\t", file)
		print("%s}%s" % (tabs, ending), file = file)
		objects[name] = { "count": 1, "doxy": [doxy] }
	elif len(obj) == 3 and sbsum == 1 and cbsum == 0:
		# Assume Object.Base.name [
		name = obj[2]
		name = trim_c_id(name)
		doxy = print_doxy(doxy, file = file)
		prefix = object_instance_prefix(name, ending)
		print("%s%s%s[] = {" % (tabs, prefix, name), file = file)
		objects[name] = { "count": 0, "doxy": [] }
		for line in instream:
			raw_code = raw_code + line
			if parse_include(line):
				continue
			if (code := parse_include_by_key(line, instream, sys.stdout)):
				raw_code = raw_code + code
				continue
			sbsum = sbsum + sbracket_count(line)
			cbsum = cbsum + cbracket_count(line)
			(doxy, line) = doxy_check_add(doxy, line)
			if sbsum < 1:
				print("%s}%s" % (tabs, ending), file = file)
				break
			if cbsum == 1: # Assume starting { on its own line
				objects[name]["count"] = objects[name]["count"] + 1
				objects[name]["doxy"].append(doxy)
				doxy = print_doxy(doxy, file = file)
				print("%s\t{" % tabs, file = file)
				raw_code = raw_code + parse_members(instream, cbsum, tabs + "\t\t", file)
				cbsum = 0
				print("%s\t}," % tabs, file = file)
	elif len(obj) == 2 and sbsum == 0 and cbsum == 1:
		# Assume Object.Base {
		for line in instream:
			raw_code = raw_code + line
			if parse_include(line):
				continue
			if (code := parse_include_by_key(line, instream, file)):
				raw_code = raw_code + code
				continue
			sbsum = sbsum + sbracket_count(line)
			cbsum = cbsum + cbracket_count(line)
			(doxy, line) = doxy_check_add(doxy, line)
			if cbsum < 1: # Ending } found
				break
			if sbsum == 1 and cbsum == 1 and sbracket_count(line) > 0:
				# Assume Class_name [
				tok = line.split()
				name = trim_c_id(tok[0])
				objects[name] = { "count": 0, "doxy": [] }
				doxy = print_doxy(doxy, file = file)
				prefix = object_instance_prefix(name, ending)
				print("%s%s%s[] = {" % (tabs, prefix, name), file = file)
			elif sbsum == 1 and cbsum == 2:
				# Assume class_name [ \n { \n
				objects[name]["count"] = objects[name]["count"] + 1
				objects[name]["doxy"].append(doxy)
				doxy = print_doxy(doxy, file = file)
				print("%s\t{" % tabs, file = file)
				raw_code = raw_code + parse_members(instream, 1, tabs + "\t\t", file)
				print("%s\t}," % tabs, file = file)
				cbsum = 1
			elif sbsum == 0 and cbsum == 1 and sbracket_count(line) < 0:
				# Assume ending ] of class table alone one its own line
				print("%s}%s" % (tabs, ending), file = file)
			elif sbsum == 0 and cbsum == 1 and line.count("}") == 1:
				# Assume name."1" {}
				# No init values, so no instantiation code needed, but we still need
				# to store the object into objexts dict.
				tok = line.split()
				tok = tok[0].split(".")
				name = trim_c_id(tok[0])
				if not objects.get(name):
					objects[name] = { "count": 0, "doxy": [] }
				objects[name]["count"] = objects[name]["count"] + 1
				objects[name]["doxy"].append(doxy)
				doxy = print_doxy(doxy, file = file)
			elif sbsum == 0 and cbsum == 2 and cbracket_count(line) == 1:
				# Assume name."1" {
				tok = line.split()
				tok = tok[0].split(".")
				name = trim_c_id(tok[0])
				mname = name
				if len(tok) > 1:
					idx = tok[1].strip(" \"\'")
					if idx.isidentifier() or idx.isnumeric():
						mname = mname + "_" + idx
				if not objects.get(name):
					objects[name] = { "count": 0, "doxy": [] }
				objects[name]["count"] = objects[name]["count"] + 1
				objects[name]["doxy"].append(doxy)
				doxy = print_doxy(doxy, file = file)
				prefix = object_instance_prefix(name, ending)
				print("%s%s%s = {" % (tabs, prefix, mname), file = file)
				raw_code = raw_code + parse_members(instream, 1, tabs + "\t", file)
				cbsum = 1
				print("%s}%s" % (tabs, ending), file = file)
	return raw_code

def parse_object_and_make_raw_code_page(filename, index, instream, fline):
	"""Parses an object instance outputs its C equivalent, and creates a raw code page of
	   of it and prints a reference to it. Handles an "Object.... {}" instance completely
	   and produces possibly multiple initialized C structs, but always just one raw code
	   block containing the "Object.... {}" block completely.

	Parameters:
	filename (stream): Name of the file we are paring
	index (int): The index of the decoded Object block in this file we are handling
	instream (stream): Input stream of topology2 file we are decoding
	fline (string): First input line that was read by the caller

	"""
	tok = filename.split("/")
	filename = fname = tok[len(tok)-1]
	fname = fname.replace("-", "_")
	fname = fname.replace(".", "_")
	c_instances = io.StringIO()
	raw_code = fline + parse_object(instream, fline, c_instances)
	print("/*! \\page %s_%d_rawcode The %s instances #%d in topology2 code\n\t\\code{.unparsed}" %
	      (fname, index, filename, index))
	print(raw_code)
	print("\t\\endcode\n*/")
	print("//! \\brief \\ref %s_%d_rawcode" % (fname, index))
	print(c_instances.getvalue())
	c_instances.close()

# Main starts here, apart from debug file opening
filename = sys.argv[1]
logging.info("file: %s", filename)
with open(filename, "r+", encoding="ascii") as instream:
	block_idx = 0

	shortfname = filename[filename.find("/topology2/"):]
	print("//! \\file %s" % shortfname[11:])
	print("//! Source file can be found " +
	      "<a href=\"https://github.com/thesofproject/sof/tree/main/tools/topology%s\">here</a>."
	      % shortfname)
	for line in instream:
		if parse_include(line):
			continue
		if parse_define_block(line, instream):
			continue
		if parse_include_by_key(line, instream, sys.stdout):
			continue
		if re.search(r"^\s*Class\.", line):
			parse_class(instream, line)
		elif re.search(r"^\s*Object\.", line):
			parse_object_and_make_raw_code_page(filename, block_idx, instream, line)
			block_idx = block_idx + 1
		elif line.find("##") >= 0:
			sys.stdout.write(line.replace("##", "//!", 1))
		else:
			sys.stdout.write("\n")
