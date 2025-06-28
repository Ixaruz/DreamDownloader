#!/usr/bin/env python3
import argparse
import os
import json
import time
import random
import requests
from requests.exceptions import ChunkedEncodingError
import ormsgpack
import logging

def query_dreams(host, port, params):
    url = f"http://{host}:{port}/dream_query"
    r = requests.get(url, params=params)
    r.raise_for_status()
    return ormsgpack.unpackb(r.content)

def download_dream(host, port, dream):
    dream_url = dream["contents"][0]["url"]
    meta_url = dream["meta"]
    url = f"http://{host}:{port}/dream_download"
    # POST body = full URL
    # should already be utf-8 encoded but just making sure...
    r1 = requests.post(url, data=dream_url.encode('utf-8'))
    r1.raise_for_status()
    r2 = requests.post(url, data=meta_url.encode('utf-8'))
    r2.raise_for_status()
    
    try:
        dream_body = bytearray()
        dream_meta = bytearray()
        for chunk in r1.iter_content(chunk_size=8192):
            if chunk:  # filter out keep-alive chunks
                dream_body.extend(chunk)
                
        for chunk in r2.iter_content(chunk_size=8192):
            if chunk:  # filter out keep-alive chunks
                dream_meta.extend(chunk)
    except (ChunkedEncodingError) as e:
        # server closed early: treat what we got as 'the full body'
        print(f"[!] Warning: incomplete read ({e})")

    
    return bytes(dream_body), ormsgpack.unpackb(bytes(dream_meta))

def format_da_id(numeric_id):
    # Nintendo format: DA-XXXX-XXXX-XXXX, pad to 12 digits
    s = f"{numeric_id:012d}"
    parts = [s[i:i+4] for i in range(0, 12, 4)]
    return "DA-" + "-".join(parts)

def save_dream(dream, host, port):
    # Extract ID, meta, and first content URL
    dream_id = dream.get("id")
    meta = dream.get("meta")
    contents = dream.get("contents", [])
    if not contents:
        logging.error(f" [!] No contents for dream ID {dream_id}")
        return

    da_text = format_da_id(dream_id)

    # Download dream + meta
    logging.info(f"Downloading dream {da_text}...")
    body, meta = download_dream(host, port, dream)
    
    # Timestamp formatting
    tt = meta.get("mMtCurUploadTime")
    date_time = f'{tt.get("mYear",0):04d}.{tt.get("mMonth",0):02d}.{tt.get("mDay",0):02d}@{tt.get("mHour",0):02d}-{tt.get("mMin",0):02d}'

    # Make directories
    base_dir = os.path.join(da_text, date_time)
    os.makedirs(base_dir, exist_ok=True)
    
    # Save body
    with open(os.path.join(base_dir, "dream_land.dat"), "wb") as f:
        f.write(body)

    # Save metadata
    with open(os.path.join(base_dir, "dream_land_meta.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)

def cmd_id(args):
    result = query_dreams(args.host, args.port,
                          {"id" : args.id})
    dreams = result.get("dreams", [])
    if not dreams:
        logging.error("No dream found for that ID.")
        return
    logging.info("Found dream, downloading first result...")
    save_dream(dreams[0], args.host, args.port)

def cmd_land_name(args):
    result = query_dreams(args.host, args.port,
                          {"land_name" : args.land_name})
    dreams = result.get("dreams", [])
    if not dreams:
        logging.error("No dream found for that island name.")
        return
    logging.info("Found dream, downloading first result...")
    save_dream(dreams[0], args.host, args.port)

def cmd_recommend(args):
    result = query_dreams(args.host, args.port,
                          {"recommend" : "",
                           "lang" : args.lang})
    dreams = result.get("dreams", [])
    if not dreams:
        logging.error("No recommended dreams found.")
        return
    logging.info(f"Found {len(dreams)} recommended dreams. Beginning batch download...")
    for i, dream in enumerate(dreams, 1):
        save_dream(dream, args.host, args.port)
        if i < len(dreams):
            wait = random.uniform(10.0, 20.0)
            logging.info(f"Sleeping {wait:.1f}s before next download...")
            time.sleep(wait)
    logging.info("All recommended dreams downloaded.")

def main():
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(description="ACBAA Batch Dream Downloader")
    parser.add_argument("--host", required=True, help="Switch IP or hostname")
    parser.add_argument("--port", required=True, type=int, help="Switch port")

    sub = parser.add_subparsers(dest="command", required=True)

    p_id = sub.add_parser("id", help="Download by Dream ID")
    p_id.add_argument("id", help="Numeric Dream ID")
    p_id.set_defaults(func=cmd_id)
    
    
    p_ln = sub.add_parser("land_name", help="Download by Island Name")
    p_ln.add_argument("land_name", help="Name of the island")
    p_ln.set_defaults(func=cmd_land_name)

    p_rec = sub.add_parser("recommend", help="Download all recommended dreams")
    p_rec.add_argument("lang", help="Language code (e.g. en-GB, ja)")
    p_rec.set_defaults(func=cmd_recommend)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
