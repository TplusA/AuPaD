#! /usr/bin/python3

import argparse
import json
import re
import sys
from pathlib import Path


def warning(msg):
    sys.stderr.write('WARNING: {}\n'.format(msg))


def show_error(msg):
    sys.stderr.write('ERROR: {}\n'.format(msg))


class Connectable:
    INBOUND = 1
    OUTBOUND = 2
    INOUT = INBOUND | OUTBOUND

    def __init__(self, id, signal_connectors):
        self.id = id
        assert(signal_connectors >= Connectable.INBOUND)
        assert(signal_connectors <= Connectable.INOUT)
        self.signal_connectors = signal_connectors
        self.description = None

    def set_description(self, desc):
        self.description = desc

    def __repr__(self):
        if self.signal_connectors == Connectable.INOUT:
            type = 'transfers'
        elif self.signal_connectors == Connectable.INBOUND:
            type = 'receives'
        else:
            type = 'emits'

        s = '{} ({})'.format(self.id, type)
        if self.description is not None:
            s += ', "{}"'.format(self.description)

        return s


class AudioSource(Connectable):
    def __init__(self, id):
        super().__init__(id, Connectable.OUTBOUND)
        self.parent = None

    def set_parent(self, p):
        assert(isinstance(p, AudioSource))
        self.parent = p

    def __repr__(self):
        return '{}, parent {}'\
               .format(super().__repr__(),
                       self.parent.id if self.parent is not None else 'None')


class AudioSink(Connectable):
    def __init__(self, id):
        super().__init__(id, Connectable.INBOUND)


class Control:
    def __init__(self, id, json):
        assert(isinstance(id, str))
        self.id = id
        self.label = json.get('label', None)
        self.desc = json.get('description', None)
        self.neutral_setting = json.get('neutral_setting', None)
        self.for_roon = json.get('roon', None)
        assert(isinstance(self.label, (str, type(None))))
        assert(isinstance(self.desc, (str, type(None))))

    def __repr__(self):
        s = self.id
        if self.label is not None:
            s += ' [{}]'.format(self.label)
        if self.desc is not None:
            s += ' ({})'.format(self.desc)
        if self.neutral_setting is not None:
            s += ', neutral {}'.format(self.neutral_setting)
        if self.for_roon is not None:
            s += ', reported to Roon ({})' \
                 .format(self.for_roon.get('template'))
        return s

    def has_mappings(self):
        return False


class ControlMapping:
    def __init__(self, id):
        assert(isinstance(id, str))
        self.id = id

    def __repr__(self):
        return self.id


class ControlMappingTable(ControlMapping):
    def __init__(self, id, table):
        super().__init__(id)
        assert(isinstance(table, list))
        self.__table = table

    def size(self):
        return len(self.__table)

    def __repr__(self):
        return '{} -> {}'.format(super().__repr__(), self.__table)


class ControlOnOff(Control):
    def __init__(self, id, json):
        super().__init__(id, json)
        if self.neutral_setting is not None:
            assert(isinstance(self.neutral_setting, str))
            assert(self.neutral_setting == 'on' or
                   self.neutral_setting == 'off')

    def __repr__(self):
        s = super().__repr__()
        s += ' - OnOff'
        return s


class ControlChoice(Control):
    def __init__(self, id, json):
        super().__init__(id, json)
        self.choices = json['choices']
        if self.neutral_setting is not None:
            assert(isinstance(self.neutral_setting, str))
            assert(self.neutral_setting in self.choices)

    def __repr__(self):
        s = super().__repr__()
        s += ' - Choice {{{}}}'\
             .format(', '.join([str(c) for c in self.choices]))
        return s


class ControlRange(Control):
    def __init__(self, id, json):
        super().__init__(id, json)
        self.value_type = json['value_type']
        self.min = json['min']
        self.max = json['max']
        self.step = json['step']
        self.scale = json['scale']
        self.mappings = []
        assert(isinstance(self.min, (int, float)))
        assert(isinstance(self.max, (int, float)))
        assert(self.min <= self.max)
        assert(isinstance(self.step, (int, float)))
        assert(isinstance(self.scale, str))

        if self.neutral_setting is not None:
            assert(isinstance(self.neutral_setting, (int, float)))
            assert(self.neutral_setting >= self.min and
                   self.neutral_setting <= self.max)

        mappings = json.get('mapped_to_scales', None)
        if mappings is not None:
            for m in mappings:
                self.mappings.append(_mk_control_mapping(m, mappings[m]))

        for m in self.mappings:
            s = (self.max - self.min + 1) / self.step
            assert(m.size() == s)

    def __repr__(self):
        s = super().__repr__()
        s += ' - Range [{}..{}] +{} {}, type {}' \
             .format(self.min, self.max, self.step, self.scale,
                     self.value_type)
        return s

    def has_mappings(self):
        return False if not self.mappings else True


def _mk_control(id, json):
    assert(isinstance(id, str))

    type = json['type']
    assert(isinstance(type, str))

    if type == 'on_off':
        return ControlOnOff(id, json)
    elif type == 'choice':
        return ControlChoice(id, json)
    elif type == 'range':
        return ControlRange(id, json)

    raise RuntimeError('Invalid control type {}'.format(type))


def _mk_control_mapping(id, json):
    assert(isinstance(id, str))

    if 'table' in json:
        return ControlMappingTable(id, json['table'])

    raise RuntimeError('Invalid mapping {}'.format(json))


class Element(Connectable):
    def __init__(self, id):
        super().__init__(id, Connectable.INOUT)
        self.controls = []

    def add_control(self, ctl):
        assert(isinstance(ctl, Control))
        self.controls.append(ctl)

    def has_controls(self):
        return True if self.controls else False


class IOMapping:
    def __init__(self, select):
        self.select = select


class IOMapMux(IOMapping):
    def __init__(self, select, table=None):
        super().__init__(select)
        self.table = table


class IOMapDemux(IOMapping):
    def __init__(self, select):
        super().__init__(select)


class IOMapTable(IOMapping):
    def __init__(self, select, table):
        super().__init__(select)
        self.table = table


class DeviceModel:
    def __init__(self, id):
        self.id = id
        self._clear()

    def _clear(self):
        self.audio_sources = None
        self.audio_sinks = None
        self.elements = None
        self.audio_signal_paths = None
        self.signal_mappings = None
        self.signal_types = None
        self.usb_connectors = None
        self.invalid = True

    _VALID_PROPS = ('audio_sources', 'audio_sinks', 'elements',
                    'audio_signal_paths', 'signal_types', 'usb_connectors')

    def from_json(self, json, models=None):
        self._clear()

        props = json.get('copy_properties', None)
        if props:
            deps = set()
            copy_props = set()
            copy_all = False

            for p in props:
                if p in DeviceModel._VALID_PROPS:
                    deps.add(props[p])
                    copy_props.add(p)
                elif p == 'all':
                    deps.add(props[p])
                    copy_all = True
                else:
                    raise RuntimeError(
                        'Cannot copy from "{}", invalid source property '
                        'specification'.format(p))

            if copy_props and copy_all:
                raise RuntimeError(
                    'Cannot copy \"all\" *and* from individual fields')

            if not models:
                return deps

            if copy_all:
                copy_props = DeviceModel._VALID_PROPS
                copy_from = props['all']
            else:
                copy_from = None

            for p in copy_props:
                src = models[props.get(p, copy_from)]
                setattr(self, p, getattr(src, p))

        if self.audio_sources is None:
            self.audio_sources = \
                DeviceModel.__parse_audio_sources(json['audio_sources'])
            DeviceModel.__resolve_audio_source_parent_relations(
                json['audio_sources'], self.audio_sources)

        if self.audio_sinks is None:
            self.audio_sinks = \
                DeviceModel.__parse_audio_sinks(json['audio_sinks'])

        if self.elements is None:
            self.elements = DeviceModel.__parse_elements(json['elements'])

        sp, sm = \
            DeviceModel.__parse_signal_paths(json.get('audio_signal_paths'),
                                             self.id)

        if self.audio_signal_paths is None:
            self.audio_signal_paths = sp

        if self.signal_mappings is None:
            self.signal_mappings = sm

        self.invalid = False
        return None

    @staticmethod
    def __parse_audio_sources(json):
        result = {}

        for a in json:
            src = AudioSource(a['id'])

            if src.id in result:
                raise RuntimeError(
                    'Duplicate audio source {}'.format(src.id))

            result[src.id] = src

        return result

    @staticmethod
    def __parse_audio_sinks(json):
        result = {}

        for a in json:
            sink = AudioSink(a['id'])

            if sink.id in result:
                raise RuntimeError(
                    'Duplicate audio sink {}'.format(sink.id))

            result[sink.id] = sink

        return result

    @staticmethod
    def __resolve_audio_source_parent_relations(json, audio_sources):
        for a in json:
            p = a.get('parent', None)
            if p is None:
                continue

            if p not in audio_sources:
                raise RuntimeError(
                    'Parent audio source {} does not exist'.format(p))

            audio_sources[a['id']].set_parent(audio_sources[p])

    @staticmethod
    def __parse_elements(json):
        result = {}

        for a in json:
            try:
                elem = Element(a['id'])

                if elem.id in result:
                    raise RuntimeError(
                        'Duplicate signal path element {}'.format(elem.id))

                if DeviceModel.__parse_element(a.get('element', None), elem):
                    result[elem.id] = elem
            except:  # noqa: E722
                show_error('Failed parsing element {}'.format(elem))
                raise

        return result

    @staticmethod
    def __parse_element(json, elem):
        if not json:
            return False

        desc = json.get('description', None)
        if desc is not None:
            elem.set_description(desc)

        controls = json.get('controls', None)
        if controls is not None:
            for ctl in controls:
                elem.add_control(_mk_control(ctl, controls[ctl]))

        if json.get('predefined', None) is not None:
            warning('Element templates are not supported yet ({})'
                    .format(elem.id))

        return True

    @staticmethod
    def __parse_signal_paths(json, model_id):
        if not json:
            return None, None

        edges = []
        maps = []

        for block in json:
            conns = block.get('connections')
            iomap = block.get('io_mapping')

            if conns and iomap:
                raise RuntimeError('Found "connections" and "io_mapping" in '
                                   'same object ({})'.format(model_id))

            if conns:
                for c in conns:
                    def ensure_connector_name(n, cname, expr):
                        return n if re.search(expr, n) else n + cname

                    # a = c if re.search(r'\.out[0-9]+', c) else c + '.out0'
                    a = ensure_connector_name(c, '.out0', r'\.out[0-9]+')

                    if isinstance(conns[c], list):
                        for b in conns[c]:
                            edges.append(
                                (a, ensure_connector_name(
                                        b, '.in0', r'\.in[0-9]+')))
                    else:
                        edges.append(
                            (a, ensure_connector_name(
                                    conns[c], '.in0', r'\.in[0-9]+')))

            if iomap:
                map_type = iomap['mapping']

                if map_type == 'mux':
                    maps.append(IOMapMux(iomap['select'],
                                         iomap.get('mapping_table', None)))
                elif map_type == 'demux':
                    maps.append(IOMapDemux(iomap['select']))
                elif map_type == 'table':
                    maps.append(IOMapTable(iomap['select'],
                                           iomap['mapping_table']))
                else:
                    raise RuntimeError('Unknown mapping type "{}"'
                                       .format(map_type))

        return edges, maps


def _dump_model(model):
    if model.invalid:
        print('{}\n  *Invalid*'.format(model.id))
        return

    print('{}\n  Sources:\n{}\n  Sinks:\n{}\n  Elements:'
          .format(model.id,
                  '\n'.join(['    {}'.format(s)
                             for s in model.audio_sources.values()]),
                  '\n'.join(['    {}'.format(s)
                             for s in model.audio_sinks.values()])))

    for e in model.elements.values():
        print('    {}'.format(e))
        if not e.has_controls():
            continue

        print('      Controls:')
        for c in e.controls:
            print('        {}'.format(c))

            if c.has_mappings():
                print('          Mappings:')
                for m in c.mappings:
                    print('            {}'.format(m))


def _emit_dot(model, outfile):
    outfile.write('digraph {} {{\n'.format(model.id))

    switches = {}

    if model.signal_mappings:
        for sm in model.signal_mappings:
            elem, ctrl = sm.select.split('@', 1)

            if elem not in switches:
                switches[elem] = [ctrl]
            else:
                switches[elem].append(ctrl)

    if model.audio_signal_paths:
        elements = set()
        sources = set()
        sinks = set()
        arcs_drawn = set()

        for p in model.audio_signal_paths:
            outfile.write('  "{}" -> "{}" [color=blue];\n'.format(p[0], p[1]))
            sources.add(p[0])
            sinks.add(p[1])
            e_from = p[0].rsplit('.', 1)[0]
            e_to = p[1].rsplit('.', 1)[0]
            elements.add(e_from)
            elements.add(e_to)

            if not (e_from, p[0]) in arcs_drawn:
                arcs_drawn.add((e_from, p[0]))
                outfile.write(
                    '  "{}" -> "{}" [style=bold, dir=both, arrowhead=inv, '
                    'arrowtail=inv];\n'.format(e_from, p[0]))

            if not (p[1], e_to) in arcs_drawn:
                arcs_drawn.add((p[1], e_to))
                outfile.write(
                    '  "{}" -> "{}" [style=bold, dir=both, arrowhead=inv, '
                    'arrowtail=inv];\n'.format(p[1], e_to))

        for s in model.audio_sources:
            if s not in elements:
                src = model.audio_sources[s]
                if src.parent:
                    elements.add(src.id)
                    outfile.write(
                        '  "{}" -> "{}" [style=bold, dir=none];\n'
                        .format(src.id, src.parent.id))

        for n in sources:
            outfile.write(
                '  "{}" [label="{}"];\n'.format(n, n.rsplit('.', 1)[1]))

        for n in sinks:
            outfile.write(
                '  "{}" [label="{}"];\n'.format(n, n.rsplit('.', 1)[1]))

        for e in elements:
            if e in switches:
                if e in model.elements:
                    label_option = \
                        ', label="{}\\n(switched by {})"' \
                        .format(e, ', '.join(
                            ['\\"' + x + '\\"' for x in switches[e]]))
                else:
                    warning('IO mapping defined for "{}", but is '
                            'not an element'.format(e))
            else:
                label_option = ''

            if e in model.elements:
                outfile.write(
                    '  "{}" [shape=box, style=filled, fillcolor=green{}];\n'
                    .format(e, label_option))
            elif e in model.audio_sources:
                outfile.write(
                    '  "{}" [shape=box, style=filled, fillcolor=yellow];\n'
                    .format(e))
            elif e in model.audio_sinks:
                outfile.write(
                    '  "{}" [shape=box, style=filled, fillcolor=orange];\n'
                    .format(e))
            else:
                warning('Undefined element {}.{} used in signal paths'
                        .format(model.id, e))
                outfile.write(
                    '  "{}" [shape=octagon, style=filled, fillcolor=red, '
                    'label="UNDEF: {}"];\n'.format(e, e))

        for e in model.elements:
            if e not in elements:
                warning('Element "{}" is not on any signal path ({})'
                        .format(e, model.id))
                outfile.write(
                    '  "{}" [shape=octagon, style=filled, fillcolor=red, '
                    'label="UNUSED: {}"];\n'.format(e, e))

        for s in model.audio_sources:
            if s not in elements:
                warning('Audio source "{}" is not on any signal path ({})'
                        .format(s, model.id))
                outfile.write(
                    '  "{}" [shape=box, style=filled, fillcolor=red, '
                    'label="UNUSED SOURCE:\\n{}"];\n'.format(s, s))

        for s in model.audio_sinks:
            if s not in elements:
                warning('Audio sink "{}" is not on any signal path ({})'
                        .format(s, model.id))
                outfile.write(
                    '  "{}" [shape=box, style=filled, fillcolor=red, '
                    'label="UNUSED SINK:\\n{}"];\n'.format(s, s))

    outfile.write('}\n')


def _parse_all_devices(all):
    models = {}
    pending_models = {}

    for dev in all:
        model = DeviceModel(dev)
        try:
            dependencies = model.from_json(all[dev])
            if not dependencies:
                models[dev] = model
            else:
                pending_models[dev] = (model, dependencies)
        except:  # noqa: E722
            show_error('Failed parsing specification for device {}'
                       .format(model.id))
            raise

    while pending_models:
        resolved_any = False

        for m in pending_models:
            can_resolve = True
            for dep in pending_models[m][1]:
                if dep not in models:
                    can_resolve = False
                    break

            if not can_resolve:
                continue

            model = pending_models[m][0]
            del pending_models[m]
            model.from_json(all[m], models)
            models[m] = model
            resolved_any = True
            break

        if not resolved_any:
            raise RuntimeError('Unresolved models: {}'
                               .format(', '.join(pending_models.keys())))

    return models


def main():
    parser = argparse.ArgumentParser(description='Show device models')

    parser.add_argument(
            'JSON', type=Path,
            help='file containing audio path models of T+A appliances')
    parser.add_argument(
            '--device-id', '-i', metavar='ID', type=str, default=None,
            help='restrict output to given device ID')
    parser.add_argument(
            '--dot', '-d', metavar='FILE', type=Path, default=None,
            help='write dot graph for signal paths defined for given '
            'device ID (requires --device-id); pass - to write to stdout')
    args = parser.parse_args()
    options = vars(args)

    if options['dot'] is not None and options['device_id'] is None:
        parser.error('option --device-id is required if --dot is specified.')

    j = json.load(options['JSON'].open())
    models = _parse_all_devices(j['all_devices'])

    device_id = options['device_id']

    if device_id is not None:
        dotname = options['dot']

        if dotname is None:
            _dump_model(models[device_id])
        else:
            _emit_dot(models[device_id],
                      dotname.open(mode='w') if str(dotname) != '-'
                      else sys.stdout)
    else:
        for m in models.values():
            _dump_model(m)


if __name__ == '__main__':
    main()
