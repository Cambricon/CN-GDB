/*
   CAMBRICON CNGDB Copyright(C) 2018 cambricon Corporation
   This file is part of cngdb.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <signal.h>
#include <defs.h>
#include <time.h>
#include <objfiles.h>
#include "block.h"
#include "bfd.h"                /* Binary File Description */
#include "gdbthread.h"
#include "language.h"
#include "demangle.h"
#include "regcache.h"
#include "arch-utils.h"
#include "buildsym.h"
#include "valprint.h"
#include "command.h"
#include "gdbcmd.h"
#include "observer.h"
#include "linux-nat.h"
#include "inf-child.h"
#include "cngdb-linux-nat.h"
#include "cngdb-util.h"
#include "infrun.h"
#include "cngdb-tdep.h"
#include "cngdb-notification.h"
#include "cngdb-command.h"
#include "gdbthread.h"
#include "cngdb-paralle.h"
#include "symfile.h"
#include <alloca.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "cngdb-thread.h"

struct cngdb_thread_chain *device_threads = NULL;

char socket_path[100];

extern struct thread_info *thread_list;

/* thread lock, every operator on device_threads has to lock it */
pthread_mutex_t multi_thread_mutex;

/* cngdb_is_offdevice_thread() : is ptid an thread that just out device */
struct cngdb_thread_chain *
cngdb_find_thread_by_ptid (ptid_t ptid)
{
  cngdb_thread_lock ();
  struct cngdb_thread_chain *cur = device_threads;
  struct cngdb_thread_chain *res = NULL;
  while (cur)
    {
      if (ptid_equal (cur->tinfo->ptid, ptid))
        {
           res = cur;
           break;
        }
      cur = cur->next;
    }
  cngdb_thread_unlock ();
  return res;
}

struct cngdb_thread_chain *
cngdb_find_thread_by_status (int status)
{
  cngdb_thread_lock ();
  struct cngdb_thread_chain* cur = device_threads;
  struct cngdb_thread_chain *res = NULL;
  while (cur)
    {
      if (cur->thread_status & status)
        {
           res = cur;
           break;
        }
      cur = cur->next;
    }
  cngdb_thread_unlock ();
  return res;
}

void
cngdb_remove_offdevice_thread (ptid_t ptid)
{
  cngdb_thread_lock ();
  struct cngdb_thread_chain* cur = device_threads;
  struct cngdb_thread_chain* last = NULL;
  while (cur)
    {
      if (ptid_equal (cur->tinfo->ptid, ptid))
        {
           if (!last)
             {
               device_threads = cur->next;
             }
           else
             {
               last->next = cur->next;
             }
           free (cur);
           break;
        }
      last = cur;
      cur = cur->next;
    }
  cngdb_thread_unlock ();
}

void
cngdb_clean_thread (void)
{
  cngdb_thread_lock ();
  if (!device_threads)
    {
      cngdb_thread_unlock ();
      return;
    }
  struct cngdb_thread_chain* cur = device_threads;
  struct cngdb_thread_chain* next = cur->next;
  free (cur);
  while (next)
    {
      cur = next;
      next = cur->next;
      free (cur);
    }
  device_threads = NULL;
  cngdb_thread_unlock ();
}

void
cngdb_thread_lock (void)
{
  pthread_mutex_lock (&multi_thread_mutex);
}

void
cngdb_thread_unlock (void)
{
  pthread_mutex_unlock (&multi_thread_mutex);
}

void
cngdb_thread_mark (struct cngdb_thread_chain * thread,
                   enum cngdb_thread_status status)
{
  cngdb_thread_lock ();
  thread->thread_status = status;
  cngdb_thread_unlock ();
}

static void
cngdb_unlink_data (void)
{
  unlink (socket_path);
}

void *
cngdb_thread_monitoring (void* args)
{
  pthread_mutex_init (&multi_thread_mutex, NULL);
  struct sockaddr_un servaddr;
  int listen_fd;
  if ((listen_fd = socket (PF_UNIX, SOCK_STREAM, 0)) < 0)
    {
       CNGDB_FATAL ("init socket failed");
       return NULL;
    }
  #define CNGDB_IPC_ALL "/tmp/.cngdb/cngdb-ipc-tid."
  snprintf (socket_path, sizeof(socket_path), "%s%d", CNGDB_IPC_ALL, getpid ());
  #undef CNGDB_IPC_ALL
  CNGDB_INFO (" socket path : %s", socket_path);
  memset (&servaddr, 0, sizeof (struct sockaddr_un));
  servaddr.sun_family = AF_UNIX;
  snprintf (servaddr.sun_path, sizeof(servaddr.sun_path), "%s", socket_path);
  if (bind (listen_fd, &servaddr, sizeof (struct sockaddr_un)) < 0)
    {
      CNGDB_FATAL ("bind socket failed");
      return NULL;
    }
  listen (listen_fd, 0);
  pthread_cleanup_push (cngdb_unlink_data, NULL);

  while (1)
    {
      struct data_transfer data;
      struct data_transfer* buf = &data;
      int conn_fd = accept (listen_fd, NULL, NULL);
      if (conn_fd == -1)
        {
          CNGDB_FATAL ("accept wrong!");
          return NULL;
        }

      /* fill msg desc */
      struct cmsghdr *cmsg;
      struct msghdr msg;
      struct iovec iov;

      cmsg = alloca(sizeof (struct cmsghdr) + sizeof (int));
      cmsg->cmsg_len = sizeof (struct cmsghdr) + sizeof (int);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;

      iov.iov_base = buf;
      iov.iov_len = sizeof(struct data_transfer);

      msg.msg_control = cmsg;
      msg.msg_controllen = cmsg->cmsg_len;
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      msg.msg_name = NULL;
      msg.msg_namelen = 0;

      recvmsg(conn_fd, &msg, 0);
      if (buf->type != STYPE_TID)
        {
          CNGDB_FATAL ("transfer tid error");
          return NULL;
        }

      int tid = ((int *)buf->data)[0];
      CNGDB_INFOD (CNDBG_DOMAIN_THREAD, " get debug thread id : %d", tid);

      cngdb_thread_lock ();
      pthread_cleanup_push (cngdb_thread_unlock, NULL);
      struct thread_info *tp;
      for (tp = thread_list; tp; tp = tp->next)
        {
          char dir[100];
          /* check if thread dir exist */
          if (cngdb_get_thread_id (tp->ptid) == tid)
            {
              struct cngdb_thread_chain *dt =
                  (struct cngdb_thread_chain *) xmalloc
                  (sizeof (struct cngdb_thread_chain));
              dt->tinfo = tp;
              dt->next = NULL;
              dt->thread_status = CNDBG_THREAD_WAITING;

              struct cngdb_thread_chain *cur = device_threads;
              if (!cur)
                {
                  device_threads = dt;
                }
              else
                {
                  while (cur->next)
                    cur = cur->next;
                  cur->next = dt;
                }
              CNGDB_INFOD (CNDBG_DOMAIN_THREAD,
                           "update thread info sucess, update thread : %d",
                           tid);
              break;
            }
        }
      if (!tp)
        {
          CNGDB_FATAL ("unknown thread id, monitioring finished");
          return NULL;
        }
      cngdb_thread_unlock ();
      pthread_cleanup_pop (1);
      conn_fd = accept (listen_fd, NULL, NULL);
      sendmsg (conn_fd, &msg, 0);
    }
  cngdb_unlink_data ();
  pthread_cleanup_pop (1);
  return NULL;
}
