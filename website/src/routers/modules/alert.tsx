import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaBell } from "react-icons/fa";

export default [
  {
    path: "/alert",
    name: "报警管理",
    icon: <FaBell />,
    element: <LayoutIndex />,
    meta: {
      rank: 2
    },
    children: [
      {
        index: true,
        name: "报警列表",
        element: lazyLoad(lazy(() => import("@/pages/Alert/List"))),
      },
    ],
  },
] as RouteObject;

