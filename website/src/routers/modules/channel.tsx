import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaHdd } from "react-icons/fa";

export default [
  {
    path: "/channel",
    name: "通道管理",
    icon: <FaHdd />,
    element: <LayoutIndex />,
    meta: {
      rank: 0
    },
    children: [
      {
        index: true,
        name: "通道列表",
        element: lazyLoad(lazy(() => import("@/pages/Channel/List"))),
      },
      {
        path: "algorithm/:channelId",
        name: "算法配置",
        element: lazyLoad(lazy(() => import("@/pages/AlgorithmConfig"))),
        meta: {
          hideInMenu: true,
        },
      },
    ],
  },
] as RouteObject;
