#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

size_t write_data (void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t written = fwrite (ptr, size, nmemb, stream);
  return written;
}

std::string get_filename (const std::string &str)
{
  std::size_t found = str.find_last_of ("/\\");
  return str.substr (found + 1);
}

int main (int argc, char *argv[])
{
  std::cout << argc - 1 << " file(s) will be downloaded.\n";

  std::vector<CURL *> easy_curls;
  std::vector<FILE *> fpointers;

  for (int i = 1; i < argc; i++)
    {
      //      CURLcode res;
      const char *url         = argv[i];
      std::string output_name = get_filename (argv[i]);
      std::cout << "Downloading " << output_name << "...\n";

      auto curl = curl_easy_init ();
      easy_curls.push_back (curl);

      if (curl)
        {
          fpointers.push_back (fopen (output_name.c_str (), "wb"));
          curl_easy_setopt (curl, CURLOPT_URL, url);
          curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_data);
          curl_easy_setopt (curl, CURLOPT_WRITEDATA, fpointers.back ());
          curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
          //                    res = curl_easy_perform(curl);
        }
    }
  //  return 0;
  //}
  CURLM *multi_handle;

  /* init a multi stack */
  multi_handle = curl_multi_init ();

  /* add the individual transfers */
  for (auto curl : easy_curls)
    curl_multi_add_handle (multi_handle, curl);

  int still_running;
  do
    {
      struct timeval timeout;
      int rc;       /* select() return code */
      CURLMcode mc; /* curl_multi_fdset() return code */

      fd_set fdread;
      fd_set fdwrite;
      fd_set fdexcep;
      int maxfd = -1;

      long curl_timeo = -1;

      FD_ZERO (&fdread);
      FD_ZERO (&fdwrite);
      FD_ZERO (&fdexcep);

      /* set a suitable timeout to play around with */
      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;

      curl_multi_timeout (multi_handle, &curl_timeo);
      if (curl_timeo >= 0)
        {
          timeout.tv_sec = curl_timeo / 1000;
          if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;
          else
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

      /* get file descriptors from the transfers */
      mc = curl_multi_fdset (multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

      if (mc != CURLM_OK)
        {
          fprintf (stderr, "curl_multi_fdset() failed, code %d.\n", mc);
          break;
        }

      if (maxfd == -1)
        {
          struct timeval wait = {0, 100 * 1000}; /* 100ms */
          rc                  = select (0, NULL, NULL, NULL, &wait);
        }
      else
        {
          rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }

      switch (rc)
        {
        case -1:
          /* select error */
          break;
        case 0:  /* timeout */
        default: /* action */
          curl_multi_perform (multi_handle, &still_running);
          break;
        }
    }
  while (still_running);

  int i = 1;
  for (auto curl : easy_curls)
    {
      curl_multi_remove_handle (multi_handle, curl);
      double filesize = 0.;
      int res         = curl_easy_getinfo (curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
      if ((CURLE_OK == res) && (filesize > 0.0))
        std::cout << argv[i] << " Downloaded! Size:" << filesize << " bytes\n";
      else if (filesize < 0)
        std::cout << argv[i] << " Downloaded! Size is unknown :(\n";
      curl_easy_cleanup (curl);
      i++;
    }

  curl_multi_cleanup (multi_handle);

  for (auto fp : fpointers)
    fclose (fp);

  for (int i = 1; i < argc; i++)
    {
      auto filename = get_filename (argv[i]);
      std::ifstream file (filename, std::ios::binary | std::ios::ate);
      std::cout << filename << " Result size: " << file.tellg () << " bytes\n";
    }


  std::cout << "Done!\n";
  return 0;
}
