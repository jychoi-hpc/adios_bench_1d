#!/usr/bin/env python
import xml.etree.ElementTree as ET
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-i', '--infile', help='infile', default='writer.xml')
parser.add_argument('-o', '--outfile', help='outfile', default='writer.xml')
parser.add_argument('--group', help='group name', default='writer')
parser.add_argument('method', help='method', default='POSIX')
parser.add_argument('params', help='params', default='local-fs=1,verbose=3')
args = parser.parse_args()

tree = ET.parse(args.infile)
root = tree.getroot()

for x in root.findall('transport'):
    if x.attrib['group'] == args.group:
        print 'Changing method: %s => %s'%(x.attrib['method'], args.method)
        print 'Changing params: %s => %s'%(x.text, args.params)
        x.attrib['method'] = args.method
        x.text = args.params

tree.write(args.outfile, encoding='utf-8', xml_declaration=True)
print "Saved:", args.outfile

