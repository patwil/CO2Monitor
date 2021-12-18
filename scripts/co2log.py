#!/usr/bin/env python3
# coding: utf-8

"""
	A utility to display Co2Monitor log readings
	for given date/time + duration.
"""

import sys
import os
import sys
from datetime import date, datetime
import time
import argparse
import textwrap
import re

DEBUG = False

prog_path, prog_name = os.path.split(sys.argv[0])
log_dir='/var/log/co2monitor/'

def get_date_time(date_time):
	try:
		now = datetime.now()
		dt = None
		if re.search(r'^\d{6}-\d{4}$', date_time):
			dt = datetime.strptime(date_time, '%y%m%d-%H%M')
		elif re.search(r'^\d{6}$', date_time):
			dt = datetime.strptime(date_time, '%y%m%d')
		elif re.search(r'^\d{4}$', date_time):
			t = datetime.strptime(date_time, '%H%M')
			dt = datetime(year=now.year, month=now.month, day=now.day, hour=t.hour, minute=t.minute)
		else:
			raise Exception('Invalid date and/or time format')
	except ValueError as ve:
		sys.stderr.write('Invalid date/time format: {}\n'.format(str(ve)))
	except Exception as e:
		sys.stderr.write('{}\n'.format(str(e)))
	return dt

def print_log_line(line):
	log_items = line.split(',')
	t = float(log_items[0]) * 0.01
	rh = float(log_items[1]) * 0.01
	co2 = log_items[2] + 'ppm'
	fan_state = log_items[3]
	man_auto = '(' + log_items[4] +')'
	rh_filt = float(log_items[5]) * 0.01
	co2_filt = '(' + log_items[6] +'ppm)'
	date_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(int(log_items[7])))
	print(f'{t:.2f}C, {rh:.2f}%({rh_filt:.2f}%), {co2:<7}{co2_filt:<9}, fan: {fan_state:3} {man_auto:6}, {date_str}')

def read_log_for_date_time(dt):
	log_file_name = log_dir + str(dt.year) + '/' + str(dt.month) + '/' + str(dt.day)
	if os.path.isfile(log_file_name) and os.access(log_file_name, os.R_OK):
		with open(log_file_name) as in_file:
			print(f'opened: {log_file_name}')
			timestamp = int(dt.timestamp())
			log_lines = in_file.read().splitlines()
			for log_line in log_lines:
				log_items = log_line.split(',')
				if len(log_items) < 8:
					continue
				if timestamp > int(log_items[7]):
					continue
				print_log_line(log_line)		        
	else:
		sys.stderr.write('\nNo log "{}" for {}\n'.format(log_file_name, dt.strftime('%Y-%m-%d')))

def main():
	parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter, description=textwrap.dedent('''\
		Pretty prints CO2 Monitor readings for given: date or date-time 
		 - date (yymmdd); or
		 - date-time (yymmdd-HHMM)
		 '''))
	parser.add_argument('date_time', help='date (yymmdd) or date-time (yymmdd-HHMM)')
	args = parser.parse_args()
	dt = get_date_time(args.date_time)
	if not dt:
		sys.stderr.write('\nusage: {} yymmdd-HHMM\nor:    {} yymmdd\nor:    {} HHMM\n\n'.format(prog_name, prog_name, prog_name))
		sys.exit(1)
	read_log_for_date_time(dt)

if __name__ == '__main__':
	main()


