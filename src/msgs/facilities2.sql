/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2009-10-02 20:33:37', 'JRD', 0, 668)
-- Reserved 513-529 by CVC for FB3
('2009-07-16 05:41:59', 'QLI', 1, 530)
('2008-11-28 20:27:04', 'GDEF', 2, 346)
('2009-07-03 11:27:34', 'GFIX', 3, 121)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
--
--('1996-11-07 13:39:40', 'GLTJ', 5, 1)
--('1996-11-07 13:39:40', 'GRST', 6, 1)
--
('2005-11-05 13:09:00', 'DSQL', 7, 32)
('2008-11-13 17:07:03', 'DYN', 8, 256)
--
--('1996-11-07 13:39:40', 'FRED', 9, 1)
--
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
-- Reserved 326-334 by CVC for FB3
('2009-07-27 03:59:31', 'GBAK', 12, 335)
('2009-06-05 23:07:00', 'SQLERR', 13, 970)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2006-09-10 03:04:31', 'JRD_BUGCHK', 15, 307)
--
--('1996-11-07 13:38:43', 'GJRN', 16, 241)
--
('2008-11-28 20:59:39', 'ISQL', 17, 165)
('2009-11-13 17:49:54', 'GSEC', 18, 104)
('2002-03-05 02:30:12', 'LICENSE', 19, 60)
('2002-03-05 02:31:54', 'DOS', 20, 74)
('2009-06-22 05:57:59', 'GSTAT', 21, 46)
-- Reserved 51 by CVC for FB3
('2008-09-16 01:10:30', 'FBSVCMGR', 22, 52)
('2007-12-21 19:03:07', 'UTL', 23, 2)
-- Reserved 24-69 by CVC for FB3
('2009-07-19 07:42:54', 'NBACKUP', 24, 70)
-- Reserved facility 25 for FBTRACEMGR 1-40 by CVC for FB3
stop

COMMIT WORK;
