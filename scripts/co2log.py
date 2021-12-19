#!/usr/bin/env python3
# coding: utf-8

"""
	A utility to display Co2Monitor log readings
	for given date/time + duration.
"""

import sys
import os
import sys
from datetime import date, datetime, timedelta
import time
import argparse
import textwrap
import re

DEBUG = False

prog_path, prog_name = os.path.split(sys.argv[0])
log_dir = "/var/log/co2monitor/"
#log_dir='./var_log_co2monitor/'


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
		sys.stderr.write(f'Invalid date/time format: {str(ve)}\n')
	except Exception as e:
		sys.stderr.write(f'{str(e)}\n')
	return dt

def get_duration(duration_str):
	if not re.search(r'^ [+-]\d+[dhm]$', duration_str):
		return None
	if duration_str[0:2] == ' -':
		sign = -1
	elif duration_str[0:2] == ' +':
		sign = 1
	duration_int = int(duration_str[2:-1]) * sign
	if duration_str[-1] == 'd': return timedelta(days=duration_int)
	if duration_str[-1] == 'h': return timedelta(hours=duration_int)
	if duration_str[-1] == 'm': return timedelta(minutes=duration_int)
	return None

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
	print(f'{t:.2f}C, {rh:.2f}% ({rh_filt:.2f}%), {co2:<7}{co2_filt:<9}, fan: {fan_state:3} {man_auto:6}, {date_str}')

def read_log_for_date_time(dt, duration):
	if duration.total_seconds() < 0:
		start_dt = dt + duration
		end_dt = dt
	else:
		start_dt = dt
		end_dt = dt + duration
	now = datetime.now()
	if end_dt > now: end_dt = now
	start_timestamp = int(start_dt.timestamp())
	end_timestamp = int(end_dt.timestamp())
	log_dt  = start_dt
	while log_dt <= end_dt:
		log_file_name = log_dir + str(log_dt.year) + '/' + str(log_dt.month) + '/' + str(log_dt.day)
		if os.path.isfile(log_file_name) and os.access(log_file_name, os.R_OK):
			with open(log_file_name) as in_file:
				print(f'opened: {log_file_name}')
				log_lines = in_file.read().splitlines()
				for log_line in log_lines:
					log_items = log_line.split(',')
					if len(log_items) < 8:
						continue
					if start_timestamp > int(log_items[7]):
						continue
					if end_timestamp < int(log_items[7]):
						break
					print_log_line(log_line)		        
		else:
			sys.stderr.write(f'\nNo log "{log_file_name}" for {dt.strftime("%Y-%m-%d")}\n')
			break
		log_dt = log_dt + timedelta(days=1)

def usage():
	sys.stderr.write(f'\nusage: {prog_name} yymmdd-HHMM [+duration|-duration]\nor:    {prog_name} yymmdd [+duration|-duration]\nor:    {prog_name} HHMM [+duration|-duration]\n\n')
	sys.exit(1)

def main():
	parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter, description=textwrap.dedent('''\
		Pretty prints CO2 Monitor readings for given: date or date-time 
		 - date (yymmdd); or
		 - date-time (yymmdd-HHMM)
		 for duration (default +1m) in days (d), hours (h) or minutes (m) before (-) or after (+) date-time, e.g.:
		 +2d     (2 days after date-time)
		 -1h     (1 hour before date-time)
		 +30m    (30 minutes after date-time)
		 '''))
	parser.add_argument('date_time', help='date (yymmdd) or date-time (yymmdd-HHMM)')
	parser.add_argument('duration', help='duration: +nt or -n - where n is number of days (t=d) or hours (t=h) or minutes (t=m)', nargs='?')
	# workaround for leading +/- in duration arg. Prefix with space to fool arg parser.
	for i, arg in enumerate(sys.argv):
		if((arg[0] == '-') or (arg[0] == '+')) and arg[1].isdigit: sys.argv[i] = ' ' + arg
	args = parser.parse_args()
	dt = get_date_time(args.date_time)
	if not dt: usage()
	if args.duration:
		duration = get_duration(args.duration)
	else:
		duration = get_duration(' +1m')
	if not duration: usage()
	try:
		read_log_for_date_time(dt, duration)
	except BrokenPipeError:
		pass

if __name__ == '__main__':
	main()


