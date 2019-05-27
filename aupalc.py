#! /usr/bin/env python3
#
# A simple compiler for a simple language.
# This program turns an ASCII format of AuPaL into binary AuPaL.
#

import argparse
import re
import struct
import sys
from pathlib import Path


class Emission:
    def __init__(self):
        self.bytes = bytearray()
        self.is_complete = False
        self.what = None
        self.line_number = 0

    def empty(self):
        return not self.is_complete

    def start_from(self, where):
        self.line_number = where

    def append(self, bytes):
        if self.is_complete:
            raise RuntimeError('Cannot append to complete emission')

        if isinstance(bytes, str):
            self.bytes.extend(bytes.encode('ASCII'))
            self.bytes.append(0)
            return

        try:
            self.bytes.extend(bytes)
        except TypeError:
            self.bytes.append(bytes)

    def commit(self, what):
        if self.is_complete:
            raise RuntimeError('Cannot re-commit emission')

        self.is_complete = True
        self.what = what

    def get_line(self):
        if not self.is_complete:
            raise RuntimeError('Cannot return line number for '
                               'incomplete emission')

        return self.line_number

    def get_verbose(self):
        if not self.is_complete:
            raise RuntimeError('Cannot return verbose string for '
                               'incomplete emission')

        return self.what

    def take(self):
        if not self.is_complete:
            raise RuntimeError('Cannot take data from incomplete emission')

        result = self.bytes
        self.bytes = bytearray()
        self.is_complete = False
        self.what = None
        return result


class ParseError(RuntimeError):
    pass


class ConvertToInt:
    def __init__(self, length, is_signed):
        boundary = 2 ** ((8 * length) - (1 if is_signed else 0))
        self.length = length
        self.a = -boundary if is_signed else 0
        self.b = boundary - 1

    def convert(self, s):
        result = int(s)

        if result < self.a:
            raise OverflowError('value {} is too small (minimum value is {})'
                                .format(result, self.a))

        if result > self.b:
            raise OverflowError('value {} is too large (maximum value is {})'
                                .format(result, self.b))

        return result.to_bytes(self.length, byteorder='little',
                               signed=self.a < 0)


class ConvertToBool:
    def convert(self, s):
        if s == 'True':
            return bytes(b'\01')

        if s == 'False':
            return bytes(b'\00')

        raise ValueError('invalid boolean value')


class ConvertToIEEEDouble:
    def convert(self, s):
        return bytes(struct.pack('d', float(s)))


class ConvertToString:
    def convert(self, s):
        return s


class ConvertToFixPoint:
    MIN = -511.9375
    MAX = 511.9375
    SCALED_PRECISION = 625

    def convert(self, s):
        dbl = float(s)

        if dbl < ConvertToFixPoint.MIN:
            raise OverflowError('value {} is too small (minimum value is {})'
                                .format(dbl, ConvertToFixPoint.MIN))

        if dbl > ConvertToFixPoint.MAX:
            raise OverflowError('value {} is too large (maximum value is {})'
                                .format(dbl, ConvertToFixPoint.MAX))

        uintpart = int(dbl)
        fracpart = \
            int((dbl - uintpart) * 10000 + (0.5 if dbl >= 0.0 else -0.5))

        if dbl < 0:
            uintpart = -uintpart
            fracpart = -fracpart

        if fracpart % ConvertToFixPoint.SCALED_PRECISION != 0:
            raise OverflowError('value {} cannot be represented as '
                                'fix point number'.format(dbl))

        result = \
            (uintpart << 4) + \
            int(fracpart / ConvertToFixPoint.SCALED_PRECISION) + \
            (1 << 13 if dbl < 0.0 else 0)

        return result.to_bytes(2, byteorder='little', signed=False)


def is_quoted_string(s):
    return len(s) >= 2 and \
           ((s[0] == '"' and s[-1] == '"') or (s[0] == '\'' and s[-1] == '\''))


def quoted_string(s):
    return s[1:-1] if is_quoted_string(s) else s


type_codes = {
    'y': ConvertToInt(1, False),
    'q': ConvertToInt(2, False),
    'u': ConvertToInt(4, False),
    't': ConvertToInt(8, False),
    'Y': ConvertToInt(1, True),
    'i': ConvertToInt(2, True),
    'n': ConvertToInt(4, True),
    'x': ConvertToInt(8, True),
    'b': ConvertToBool(),
    'd': ConvertToIEEEDouble(),
    'D': ConvertToFixPoint(),
    's': ConvertToString()
}


class Parser:
    STATE_EXPECTING_COMMAND = 0
    STATE_READ_INDENTED_VALUES = 1

    def __init__(self):
        self.line_number = 0
        self.state = Parser.STATE_EXPECTING_COMMAND
        self.current_command = None
        self.indent_level = 0
        self.block_assignments = None
        self.block_context = None
        self.emission = Emission()

    def error(self, message):
        raise ParseError('Error in line {}: {}.'.format(self.line_number,
                                                        message))

    def parse(self, line):
        self.line_number += 1
        line = line.rstrip()

        if not line.lstrip() or line.lstrip()[0] == '#':
            return self.emission, False

        if self.state == Parser.STATE_READ_INDENTED_VALUES and \
           self.indent_level == 0:
            self.indent_level = len(re.split(r'\S', line, 1)[0])
            if self.indent_level == 0:
                self.error('indentation expected')

        if len(line) >= self.indent_level:
            if(line[0:self.indent_level] == ' ' * self.indent_level):
                line = line[self.indent_level:]
            elif line[0] != ' ':
                self._end_of_block()
                self.line_number -= 1
                return self.emission, True
            else:
                self.error('not enough indentation')
        elif line and line[0] != ' ':
            self._end_of_block()
            self.line_number -= 1
            return self.emission, True

        if not line:
            return self.emission, False

        elif line[0] == ' ':
            self.error('too much indentation')

        line_offset = 0

        if self.state == Parser.STATE_EXPECTING_COMMAND:
            command = line[line_offset]
            line_offset += 1
            line = line[line_offset:].lstrip()

            if command == 'I':
                self.emission.start_from(self.line_number)
                self._parse_command_instance_added(line)
            elif command == 'i':
                self.emission.start_from(self.line_number)
                self._parse_command_instance_removed(line)
            elif command == 'S' or command == 'U':
                self.emission.start_from(self.line_number)
                self._parse_command_set_or_update_values(command, line)
            elif command == 'u':
                self.emission.start_from(self.line_number)
                self._parse_command_update_single_value(line)
            elif command == 'd':
                self.emission.start_from(self.line_number)
                self._parse_command_delete_single_value(line)
            elif command == 'C':
                self.emission.start_from(self.line_number)
                self._parse_command_make_connection(line)
            elif command == 'c':
                self.emission.start_from(self.line_number)
                self._parse_command_remove_connections(line)
            else:
                self.error('unknown command {}'.format(command))
        elif self.state == Parser.STATE_READ_INDENTED_VALUES:
            self._parse_block_assignment(line)
        else:
            self.error('unexpected parser state {}'.format(self.state))

        return self.emission, False

    def get_line_number(self):
        return self.line_number

    def close(self):
        if self.state == Parser.STATE_READ_INDENTED_VALUES and \
           self.block_context is not None:
            self._end_of_block()

        return self.emission

    def _parse_command_instance_added(self, line):
        if not line:
            self.emission.append((ord('I'), 0, 0))
            self.emission.commit('I (clear all)')
            return

        args = re.split(r'\s+', line)
        if len(args) != 2:
            self.error('found {} arguments for command I, expecting '
                       'either 0 or 2'.format(len(args)))

        self.emission.append(ord('I'))
        self.emission.append(args[0])
        self.emission.append(args[1])
        self.emission.commit('I (instance ID "{}", name "{}")'
                             .format(args[0], args[1]))

    def _parse_command_instance_removed(self, line):
        args = re.split(r'\s+', line)
        if len(args) != 1:
            self.error('command i requires at an instance name')

        self.emission.append(ord('i'))
        self.emission.append(args[0])
        self.emission.commit('i (instance "{}" removed)'.format(args[0]))

    def _parse_command_update_single_value(self, line):
        args = re.split(r'\s+', line, 1)
        if len(args) != 2:
            self.error('command u requires at an element name and '
                       'an assignment')

        self.emission.append(ord('u'))
        self.emission.append(args[0])
        a = self._parse_assignment(args[1])
        self._emit_assignment(a)
        self.emission.commit('u (update control "{}" for element "{}")'
                             .format(a[0], args[0]))

    def _parse_command_delete_single_value(self, line):
        args = re.split(r'\s+', line)
        if len(args) != 2:
            self.error('command d requires at an element name and '
                       'a control name')

        self.emission.append(ord('d'))
        self.emission.append(args[0])
        self.emission.append(args[1])
        self.emission.commit('d (delete value "{}" for element "{}")'
                             .format(args[1], args[0]))

    def _parse_command_make_connection(self, line):
        args = re.split(r'\s+', line)
        if len(args) != 2:
            self.error('command C requires a sink and a source')

        self.emission.append(ord('C'))
        self.emission.append(args[0])
        self.emission.append(args[1])
        self.emission.commit('C (make audio connection from "{}" to "{}")'
                             .format(args[0], args[1]))

    def _parse_command_remove_connections(self, line):
        args = re.split(r'\s+', line)
        if len(args) > 2:
            self.error('too many parameters for command c')

        from_spec = '' if len(args) == 0 else quoted_string(args[0])
        to_spec = '' if len(args) == 1 else quoted_string(args[1])

        if not from_spec and not to_spec:
            details = 'all audio connections'
        elif from_spec and not to_spec:
            details = 'audio connections from "{}"'.format(from_spec)
        elif not from_spec and to_spec:
            details = 'audio connections to "{}"'.format(to_spec)
        else:
            details = 'audio connections from "{}" to "{}"' \
                      .format(from_spec, to_spec)

        self.emission.append(ord('c'))
        self.emission.append(from_spec)
        self.emission.append(to_spec)
        self.emission.commit('c (remove {})'.format(details))

    def _parse_command_set_or_update_values(self, command, line):
        args = re.split(r'\s+', line)
        if len(args) != 1:
            self.error('command {} requires at an element name'
                       .format(command))

        self.emission.append(ord(command))
        self.emission.append(args[0])

        self.current_command = command
        self.state = Parser.STATE_READ_INDENTED_VALUES
        self.block_assignments = []
        self.block_context = args[0]

    def _parse_assignment(self, line):
        cmd = line.split('=')
        if len(cmd) != 2:
            self.error('assignment expected')

        identifier = cmd[0].strip()
        if not identifier.isidentifier():
            self.error('identifier expected on left-hand side of assignment')

        value_type_code, value = self._deconstruct(cmd[1].strip())
        return (identifier, value_type_code, value)

    def _parse_block_assignment(self, line):
        self.block_assignments.append(self._parse_assignment(line))

    def _end_of_block(self):
        command = self.current_command
        assignments = self.block_assignments
        context = self.block_context

        self.indent_level = 0
        self.current_command = None
        self.state = Parser.STATE_EXPECTING_COMMAND
        self.block_assignments = None
        self.block_context = None

        if not command:
            self.error('end of assignment block, '
                       'but don\'t know for which command')

        if command == 'S' or command == 'U':
            self.emission.append(len(assignments))

            for a in assignments:
                self._emit_assignment(a)

            self.emission.commit(
                '{} ({} values for element "{}")'
                .format(command,
                        'set all' if command == 'S' else 'update some',
                        context))
        else:
            self.error('end of assignment block, but command {} '
                       'shouldn\'t have a block'.format(command))

    def _emit_assignment(self, a):
        self.emission.append(a[0])
        self.emission.append(ord(a[1][0]))
        self.emission.append(a[2])

    def _deconstruct(self, value_spec):
        try:
            return 'i', int(value_spec)
        except ValueError:
            pass

        if is_quoted_string(value_spec):
            # string
            type_code = 's'
            value = value_spec[1:-1]
        elif value_spec == 'True' or value_spec == 'False':
            # boolean
            type_code = 'b'
            value = value_spec
        elif value_spec[0] in type_codes.keys() and \
                value_spec[1] == '(' and value_spec[-1] == ')':
            # explicit type specification
            type_code = value_spec[0]
            value = value_spec[2:-1]
        else:
            self.error('invalid value specification "{}"'.format(value_spec))

        return type_code, type_codes[type_code].convert(value)


class Translator:
    def __init__(self, input_file, output_file):
        self.input_file = \
            input_file.open() if input_file is not None else sys.stdin
        self.output_file = output_file.open(mode='w+b') \
            if output_file is not None else sys.stdout.buffer
        self.parser = Parser()
        self.verbose = Translator.swallow
        self.instructions_count = 0

    @staticmethod
    def swallow(str):
        pass

    def set_verbose(self, verbose_fn):
        self.verbose = verbose_fn

    def execute(self):
        self.verbose('Translating input...')

        for line in self.input_file:
            self._add_line(line)

        self._emit_if_possible(self.parser.close())
        self.verbose('Translation succeeded')
        self.verbose('Parsed {} lines'.format(self.parser.get_line_number()))
        self.verbose('Wrote {} instructions'.format(self.instructions_count))
        self.verbose('Done')

    def _add_line(self, line):
        while True:
            e, again = self.parser.parse(line)
            self._emit_if_possible(e)
            if not again:
                return

    def _emit_if_possible(self, e):
        if not e.empty():
            self.instructions_count += 1
            self.verbose('Line {:3}: {}'.format(e.get_line(), e.get_verbose()))
            self.output_file.write(e.take())


def main():
    parser = argparse.ArgumentParser(description='Compile AuPaL scripts')

    parser.add_argument(
            'INPUT', type=Path, nargs='?', default=None,
            help='input file containing an AuPaL script')
    parser.add_argument(
            '--output', '-o', metavar='FILE', type=Path, default=None,
            help='output file')
    parser.add_argument(
            '--verbose', '-v', action='store_true',
            help='tell what is being done')

    args = parser.parse_args()
    options = vars(args)

    translator = Translator(options['INPUT'], options['output'])

    if options['verbose']:
        translator.set_verbose(print)

    try:
        translator.execute()
    except ParseError as e:
        print(e)


if __name__ == '__main__':
    main()
