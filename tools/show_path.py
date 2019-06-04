#! /usr/bin/python3

import json
import sys


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


class DeviceModel:
    def __init__(self, id):
        self.id = id
        self.audio_sources = None
        self.audio_sinks = None
        self.elements = None

    def from_json(self, json):
        self.audio_sources =\
            DeviceModel.__parse_audio_sources(json['audio_sources'])
        DeviceModel.__resolve_audio_source_parent_relations(
            json['audio_sources'], self.audio_sources)
        self.audio_sinks = DeviceModel.__parse_audio_sinks(json['audio_sinks'])
        self.elements = DeviceModel.__parse_elements(json['elements'])

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
            elem = Element(a['id'])

            if elem.id in result:
                raise RuntimeError(
                    'Duplicate signal path element {}'.format(elem.id))

            if DeviceModel.__parse_element(a.get('element', None), elem):
                result[elem.id] = elem

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
            print('WARNING: Element templates are not supported yet ({})'
                  .format(elem.id))

        return True


def _dump_model(model):
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


def main():
    j = json.load(open('models.json'))

    all = j['all_devices']
    models = {}

    for dev in all:
        model = DeviceModel(dev)
        model.from_json(all[dev])
        models[dev] = model

    if len(sys.argv) > 1:
        _dump_model(models[sys.argv[1]])
    else:
        for m in models.values():
            _dump_model(m)


if __name__ == '__main__':
    main()
