import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaVideo } from "react-icons/fa";

export default [
  {
    path: "/stream",
    name: "流媒体配置",
    icon: <FaVideo />,
    element: <LayoutIndex />,
    meta: {
      rank: 1
    },
    children: [
      {
        index: true,
        name: "GB28181 配置",
        element: lazyLoad(lazy(() => import("@/pages/GB28181Config"))),
      },
      {
        path: "report",
        name: "上报配置",
        element: lazyLoad(lazy(() => import("@/pages/ReportConfig"))),
      },
    ],
  },
] as RouteObject;

