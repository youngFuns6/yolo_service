import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaVideo } from "react-icons/fa";

export default [
  {
    path: "/stream",
    name: "推流配置",
    icon: <FaVideo />,
    element: <LayoutIndex />,
    meta: {
      rank: 1
    },
    children: [
      {
        index: true,
        name: "推流配置",
        element: lazyLoad(lazy(() => import("@/pages/PushStreamConfig"))),
      },
      {
        path: "report",
        name: "上报配置",
        element: lazyLoad(lazy(() => import("@/pages/ReportConfig"))),
      },
    ],
  },
] as RouteObject;

