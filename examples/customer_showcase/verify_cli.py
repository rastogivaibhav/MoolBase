#!/usr/bin/env python3
"""Malformed CLI rows must exit normally before creating or modifying a database."""
import subprocess,tempfile,sys
from pathlib import Path
binary=Path(sys.argv[1]).resolve()
patches=[(3,'banana'),(3,'2147483648'),(3,'1suffix'),(3,'-1'),(5,'2'),(5,'true'),(5,'999999999999999999999999999999')]
for field,value in patches:
 with tempfile.TemporaryDirectory() as tmp:
  row=['id','observation','family','0','support','1',''];row[field]=value
  source=Path(tmp)/'input.tsv';source.write_text('\t'.join(row)+'\n')
  db=Path(tmp)/'database';p=subprocess.run([str(binary),str(db),str(source),'A','B'],capture_output=True,text=True)
  assert p.returncode==2 and 'row 1' in p.stderr,(p.returncode,p.stderr)
  assert not db.exists(),'invalid parse created database'
with tempfile.TemporaryDirectory() as tmp:
 db=Path(tmp)/'database';db.mkdir();marker=db/'sentinel';marker.write_bytes(b'preserve')
 source=Path(tmp)/'input.tsv';source.write_text('bad\n')
 p=subprocess.run([str(binary),str(db),str(source),'A','B'],capture_output=True,text=True)
 assert p.returncode==2 and marker.read_bytes()==b'preserve'
print('PASS: 7 malformed numeric rows exit 2 with row diagnostics and no database writes; existing database preserved.')
