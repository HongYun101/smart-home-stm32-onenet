// ========== 第一部分：常量与工具函数 ==========
const CONFIG = {
  AUTH_INFO: "version=2022-05-01&res=userid%2FYOUR_USER_ID&et=1907299006&method=md5&sign=YOUR_SIGN",
  PRODUCT_ID: "YOUR_PRODUCT_ID",
  DEVICE_NAME: "dev-001",
  API_BASE_URL: "https://iot-api.heclouds.com",
  DATA_REFRESH_INTERVAL: 1000,
  STATUS_QUERY_LIMIT: 1,
  STATUS_QUERY_RANGE_MS: 7 * 24 * 60 * 60 * 1000,

  // 天气 API 配置（Open-Meteo 免费，无需 key）
  WEATHER_LAT: 28.84,    // 宜宾南溪纬度
  WEATHER_LON: 104.97,   // 宜宾南溪经度
  WEATHER_URL: "https://api.open-meteo.com/v1/forecast",
  WEATHER_REFRESH_MS: 1 * 60 * 1000, // 天气每1分钟刷新一次
};

/**
 * WMO 天气码 → 中文描述 + 图标emoji
 * 参考：https://open-meteo.com/en/docs
 */
const WMO_CODE_MAP = {
  0:  { desc: '晴天', emoji: '☀️' },
  1:  { desc: '少云', emoji: '🌤️' },
  2:  { desc: '多云', emoji: '⛅' },
  3:  { desc: '阴天', emoji: '☁️' },
  45: { desc: '雾',   emoji: '🌫️' },
  48: { desc: '雾凇', emoji: '🌫️' },
  51: { desc: '小雨', emoji: '🌦️' },
  53: { desc: '中雨', emoji: '🌧️' },
  55: { desc: '大雨', emoji: '🌧️' },
  61: { desc: '小雨', emoji: '🌧️' },
  63: { desc: '中雨', emoji: '🌧️' },
  65: { desc: '大雨', emoji: '🌧️' },
  71: { desc: '小雪', emoji: '🌨️' },
  73: { desc: '中雪', emoji: '🌨️' },
  75: { desc: '大雪', emoji: '❄️' },
  77: { desc: '雪粒', emoji: '❄️' },
  80: { desc: '阵雨', emoji: '🌦️' },
  81: { desc: '中阵雨', emoji: '🌧️' },
  82: { desc: '大阵雨', emoji: '⛈️' },
  85: { desc: '小阵雪', emoji: '🌨️' },
  86: { desc: '大阵雪', emoji: '❄️' },
  95: { desc: '雷暴', emoji: '⛈️' },
  96: { desc: '雷暴+小冰雹', emoji: '⛈️' },
  99: { desc: '雷暴+大冰雹', emoji: '⛈️' },
};

/**
 * 根据天气数据生成生活建议 tips
 */
const generateWeatherTips = (code, temp) => {
  if (code === 0 && temp > 30) return '阳光强烈，注意防晒补水 ☀️';
  if (code === 0) return '天气晴朗，适合户外活动 🏃';
  if ([1, 2, 3].includes(code) && temp > 28) return '多云闷热，尽量减少户外活动 💧';
  if ([1, 2, 3].includes(code)) return '多云天气，体感舒适 🍃';
  if ([45, 48].includes(code)) return '能见度较低，出行注意安全 🚗';
  if ([51, 53, 55, 61, 63, 65, 80, 81, 82].includes(code)) return '出门记得带伞，路面湿滑 ☔';
  if ([71, 73, 75, 77, 85, 86].includes(code)) return '天气寒冷，注意保暖防滑 🧥';
  if ([95, 96, 99].includes(code)) return '雷暴天气，尽量避免外出 ⚡';
  if (temp < 10) return '气温较低，注意添衣保暖 🧣';
  return '当前天气体感舒适 🌿';
};

/**
 * 值类型转换器
 */
const valueConverter = (rawValue, type) => {
  switch (type) {
    case 'bool':
      return rawValue === 'true' || rawValue === true;
    case 'int32': {
      const n = parseInt(rawValue, 10);
      return isNaN(n) ? 0 : n;
    }
    case 'float':
    case 'double': {
      const n = parseFloat(rawValue);
      return isNaN(n) ? 0 : n;
    }
    default:
      return rawValue;
  }
};

/**
 * 生成设备状态查询的时间范围
 */
const getStatusQueryTimestamps = () => {
  const now = Date.now();
  return {
    start_time: now - CONFIG.STATUS_QUERY_RANGE_MS,
    end_time: now,
  };
};

// ========== 第二部分：Page 定义 ==========
Page({
  data: {
    /**
     * 设备属性定义 — 含 DHT11 / MQ2 / BH1750 / 继电器
     */
    deviceData: [
      // DHT11 温湿度
      { identifier: "wendu",    value: 0,     type: "float" },
      { identifier: "shidu",    value: 0,     type: "float" },
      // MQ2 烟雾传感器 — 甲烷 / 丙烷 / 烟雾
      { identifier: "jiawan",   value: 0,     type: "int32" },
      { identifier: "bingwan",  value: 0,     type: "int32" },
      { identifier: "yanwu",    value: 0,     type: "int32" },
      // BH1750 光照强度 (lux)
      { identifier: "LightLux",value: 0,     type: "int32" },
      // MQ2 气体危险检测 (0=正常 1=危险)
      { identifier: "weixian",  value: 0,     type: "int32" },
      // 执行器
      { identifier: "LED1",     value: false, type: "bool"  },
      { identifier: "LED2",     value: 0,     type: "int32" },
      { identifier: "JDQ",      value: false, type: "bool"  },
    ],

    /** 视图层绑定映射 */
    deviceValueMap: {
      wendu:    '--',
      shidu:    '--',
      jiawan:   '--',
      bingwan:  '--',
      yanwu:    '--',
      guangzhao:'--',
      weixian:   0,
      LED1:     false,
      LED2:     0,
      JDQ:      false,
    },

    /** 设备在线状态 */
    deviceStatusLog: {
      code: -1,
      data: { list: [{ status: 0 }] },
    },

    /** 天气数据 — 默认值显示"宜宾南溪" */
    weather: {
      temp: 26,
      desc: '加载中…',
      emoji: '🌤️',
      humidity: 0,
      feelsLike: 26,
      tips: '正在获取天气数据…',
      windSpeed: 0,
    },
  },

  dataRefreshTimer: null,
  weatherTimer: null,
  _started: false,   // 首次加载标志
  _lastSwitchTime: {}, // 开关冷却记录 { LED1: 时间戳, JDQ: 时间戳 }
  SWITCH_COOLDOWN: 5000, // 开关操作冷却时间（毫秒）

  onLoad() {
    this.fetchAllDeviceInfo();
    this.fetchWeather();                  // 页面加载时立即获取一次天气
    this.startDataRefreshTimer();
    this.startWeatherTimer();
  },

  onUnload() {
    this.clearDataRefreshTimer();
    this.clearWeatherTimer();
  },

  // ===================== 天气 API =====================

  /**
   * 从 Open-Meteo 获取宜宾南溪实时天气
   * 免费 API，无需 key，但需在微信小程序后台将
   * 「api.open-meteo.com」加入 request 合法域名
   */
  fetchWeather() {
    const url = `${CONFIG.WEATHER_URL}`
      + `?latitude=${CONFIG.WEATHER_LAT}&longitude=${CONFIG.WEATHER_LON}`
      + `&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m`
      + `&timezone=Asia%2FShanghai`;

    wx.request({
      url,
      method: 'GET',
      success: (res) => {
        if (res.statusCode === 200 && res.data && res.data.current) {
          const c = res.data.current;
          const code = c.weather_code;
          const wmo  = WMO_CODE_MAP[code] || { desc: '未知', emoji: '🌤️' };

          this.setData({
            weather: {
              temp:       Math.round(c.temperature_2m),
              desc:       wmo.desc,
              emoji:      wmo.emoji,
              humidity:   c.relative_humidity_2m,
              feelsLike:  Math.round(c.apparent_temperature),
              windSpeed:  Math.round(c.wind_speed_10m),
              tips:       generateWeatherTips(code, c.temperature_2m),
            },
          });
        } else {
          console.warn('[Weather] 返回数据结构异常');
        }
      },
      fail: (err) => {
        console.error('[Weather] 请求失败，使用默认天气数据。', err);
        // 失败时保留默认数据显示
      },
    });
  },

  startWeatherTimer() {
    this.clearWeatherTimer();
    this.weatherTimer = setInterval(() => {
      this.fetchWeather();
    }, CONFIG.WEATHER_REFRESH_MS);
    // Weather timer started
  },

  clearWeatherTimer() {
    if (this.weatherTimer) {
      clearInterval(this.weatherTimer);
      this.weatherTimer = null;
      // Weather timer cleared
    }
  },

  // ===================== 定时器管理 =====================

  startDataRefreshTimer() {
    this.clearDataRefreshTimer();
    this.dataRefreshTimer = setInterval(() => {
      this.fetchAllDeviceInfo();
    }, CONFIG.DATA_REFRESH_INTERVAL);
    // Timer started
  },

  clearDataRefreshTimer() {
    if (this.dataRefreshTimer) {
      clearInterval(this.dataRefreshTimer);
      this.dataRefreshTimer = null;
      // Timer cleared
    }
  },

  // ===================== 数据拉取 =====================

  fetchAllDeviceInfo() {
    this.fetchDeviceProperties();
    this.fetchDeviceStatus();
  },

  async fetchDeviceProperties() {
    const url = `${CONFIG.API_BASE_URL}/thingmodel/query-device-property`
      + `?product_id=${CONFIG.PRODUCT_ID}&device_name=${CONFIG.DEVICE_NAME}`;
    try {
      const res = await this.wxRequest({ url, method: 'GET' });
      if (res.data.code === 0 && res.data.data) {
        if (!this._started) {
          console.log('[启动] OneNET连接成功 ✅  温湿度/光照/烟雾/继电器 就绪');
          this._started = true;
        }
        // 确保 data 是数组再传入
        const dataArray = Array.isArray(res.data.data) ? res.data.data : [];
        this.updateLocalDeviceData(dataArray);
      } else {
        console.warn('[Props] 异常：', res.data.msg);
        this.showToast(res.data.msg || '数据获取异常');
      }
    } catch (error) {
      console.error('[Props] 请求失败：', error);
      this.showToast('网络请求失败');
    }
  },

  /**
   * 根据服务器响应更新本地 data
   * @param {Array} remoteDataArray - 服务器返回的属性数组
   */
  updateLocalDeviceData(remoteDataArray) {
    // 防御：确保 remoteDataArray 是有效数组
    if (!Array.isArray(remoteDataArray)) return;

    const deviceDataUpdates = {};
    const valueMapUpdates   = {};
    const now = Date.now();

    remoteDataArray.forEach(remoteItem => {
      // 防御：跳过 null/undefined 或缺少 identifier 的脏数据
      if (!remoteItem || !remoteItem.identifier) return;

      // ★ 开关冷却期：跳过服务器返回的旧值，防止弹回
      if (remoteItem.identifier === 'LED1' || remoteItem.identifier === 'JDQ') {
        const lastSwitch = this._lastSwitchTime[remoteItem.identifier] || 0;
        if (now - lastSwitch < this.SWITCH_COOLDOWN) {
          return; // 冷却中，跳过此属性，保留用户操作后的值
        }
      }

      const localIndex = this.data.deviceData.findIndex(
        localItem => localItem.identifier === remoteItem.identifier
      );
      if (localIndex !== -1) {
        const targetType     = this.data.deviceData[localIndex].type;
        const processedValue = valueConverter(remoteItem.value, targetType);
        deviceDataUpdates[`deviceData[${localIndex}].value`]      = processedValue;
        valueMapUpdates[`deviceValueMap.${remoteItem.identifier}`] = processedValue;
      }
    });

    if (Object.keys(deviceDataUpdates).length > 0) this.setData(deviceDataUpdates);
    if (Object.keys(valueMapUpdates).length > 0)   this.setData(valueMapUpdates);
  },

  async fetchDeviceStatus() {
    const { start_time, end_time } = getStatusQueryTimestamps();
    const url = `${CONFIG.API_BASE_URL}/device/status-history`
      + `?product_id=${CONFIG.PRODUCT_ID}&device_name=${CONFIG.DEVICE_NAME}`
      + `&start_time=${start_time}&end_time=${end_time}&limit=${CONFIG.STATUS_QUERY_LIMIT}`;
    try {
      const res = await this.wxRequest({ url, method: 'GET' });
      if (res.data && res.data.code === 0) {
        this.setData({ deviceStatusLog: res.data });
      }
    } catch (error) {
      console.error('[Status] 请求失败：', error);
    }
  },

  // ===================== 属性设置 =====================

  async onenet_set_device_property(event) {
    const param_name = event.currentTarget.dataset.param;
    const new_value  = event.detail.value;

    // ===== 5 秒冷却：仅对开关类型生效（LED1 / JDQ） =====
    if (param_name === 'LED1' || param_name === 'JDQ') {
      const now = Date.now();
      const last = this._lastSwitchTime[param_name] || 0;
      if (now - last < this.SWITCH_COOLDOWN) {
        wx.showToast({ title: '操作过快，请5秒后再试', icon: 'none', duration: 1500 });
        this.setData({ [`deviceValueMap.${param_name}`]: !new_value });
        return;
      }
      // ★ 立即标记冷却时间，防止后续轮询覆盖此开关的值
      this._lastSwitchTime[param_name] = now;
    }

    wx.showLoading({ title: '设置中...', mask: true });

    const requestData = {
      product_id:  CONFIG.PRODUCT_ID,
      device_name: CONFIG.DEVICE_NAME,
      params: { [param_name]: new_value },
    };

    try {
      const res = await this.wxRequest({
        url:    `${CONFIG.API_BASE_URL}/thingmodel/set-device-property`,
        method: 'POST',
        data:   requestData,
      });
      console.log('[Set] 返回：', res.data);
      wx.hideLoading();

      if (res.data && res.data.code === 0) {
        this.showToast('设置成功', 'success');
        this.updatePropertyImmediately(param_name, new_value);
        this.startDataRefreshTimer();
      } else {
        this.showToast(res.data.msg || '设置失败');
      }
    } catch (error) {
      console.error('[Set] 失败：', error);
      wx.hideLoading();
      this.showToast('网络错误，设置失败');
    }
  },

  /**
   * 属性设置成功后立即更新本地数据
   */
  updatePropertyImmediately(identifier, newValue) {
    const localIndex = this.data.deviceData.findIndex(item => item.identifier === identifier);
    if (localIndex !== -1) {
      this.setData({
        [`deviceData[${localIndex}].value`]: newValue,
        [`deviceValueMap.${identifier}`]:    newValue,
      });
    }
  },

  // ===================== 基础工具 =====================

  /**
   * 封装 wx.request，自动拼接 OneNET 鉴权头，返回 Promise
   */
  wxRequest(options) {
    return new Promise((resolve, reject) => {
      wx.request({
        ...options,
        header: {
          'Authorization': CONFIG.AUTH_INFO,
          'content-type':  'application/json',
          ...options.header,
        },
        success: res  => resolve(res),
        fail:    err  => reject(err),
      });
    });
  },

  showToast(title, icon = 'none', mask = false) {
    wx.showToast({
      title,
      icon,
      duration: icon === 'success' ? 1500 : 2000,
      mask,
    });
  },
});
