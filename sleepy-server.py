#!/usr/bin/env python3
import asyncio
import logging
from datetime import datetime

from aiohttp import web

routes = web.RouteTableDef()


@routes.get(r"/{delay:(\d*\.)?\d+}")
async def delay(request):
    delay = float(request.match_info["delay"])
    logging.info(f"got request, delay={delay}")
    time1 = datetime.utcnow().isoformat(sep=" ", timespec="milliseconds")

    await asyncio.sleep(delay)
    time2 = datetime.utcnow().isoformat(sep=" ", timespec="milliseconds")

    return web.Response(text=f"Slept {delay} s from {time1} to {time2}")


def main():
    logging.basicConfig(format="%(asctime)s - %(message)s", level=logging.INFO)
    app = web.Application()
    app.add_routes(routes)
    web.run_app(app, port=8081)


if __name__ == "__main__":
    main()
