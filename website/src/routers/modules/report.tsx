import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaPaperPlane } from "react-icons/fa";

export default [
  {
    path: "/report",
    name: "上报配置",
    icon: <FaPaperPlane />,
    element: <LayoutIndex />,
    meta: {
      rank: 1
    },
    children: [
      {
        index: true,
        name: "上报配置",
        element: lazyLoad(lazy(() => import("@/pages/ReportConfig"))),
      },
    ],
  },
] as RouteObject;

