# MotionSense Web Interface Documentation

Complete documentation for the MotionSense web interface.

## 📚 Documentation Index

### 🚀 Getting Started
- **[Quick Start Guide](QUICKSTART.md)** - Get up and running in minutes
- **[Main Documentation](README.md)** - Complete API reference and architecture
- **[API Testing Guide](API_TESTING.md)** - Comprehensive testing examples

## 📖 Documentation Overview

### Quick Start Guide (`QUICKSTART.md`)
**For:** End users, first-time setup
**Contains:**
- Step-by-step setup instructions
- Interface overview
- Troubleshooting common issues
- Mobile usage tips
- Performance optimization

### Main Documentation (`README.md`)
**For:** Developers, system administrators
**Contains:**
- Complete API reference
- Architecture overview
- Development guidelines
- Browser compatibility
- Security considerations

### API Testing Guide (`API_TESTING.md`)
**For:** Developers, QA testers
**Contains:**
- Testing tools and examples
- Response validation
- Error handling
- Performance testing
- Debugging tips

## 🎯 Quick Navigation

### For End Users
1. Start with **[Quick Start Guide](QUICKSTART.md)**
2. Use **[Main Documentation](README.md)** for reference
3. Check troubleshooting section if you encounter issues

### For Developers
1. Read **[Main Documentation](README.md)** for architecture
2. Use **[API Testing Guide](API_TESTING.md)** for development
3. Reference API examples in main documentation

### For System Administrators
1. Review **[Main Documentation](README.md)** for deployment
2. Check security considerations
3. Use **[API Testing Guide](API_TESTING.md)** for monitoring

## 🔗 API Endpoints Summary

| Endpoint | Method | Description | Documentation |
|----------|--------|-------------|---------------|
| `/api/motion_counts` | GET | Daily motion counts | [README.md](README.md#1-motion-counts-api) |
| `/api/video_segments` | GET | Video segments by date | [README.md](README.md#2-video-segments-api) |
| `/api/motion_points` | GET | Motion detection points | [README.md](README.md#3-motion-points-api) |
| `/api/stream` | GET | Live MJPEG stream | [README.md](README.md#4-live-stream-api) |

## 🛠️ Development Resources

### Code Structure
```
www/html/
├── index.html              # Main application
├── css/                    # Stylesheets
├── js/                     # JavaScript modules
├── components/             # UI components
└── *.md                   # Documentation
```

### Key Files
- `app.js` - Main application controller
- `stream.js` - Live stream management
- `calendar.js` - Calendar and heatmap
- `playback.js` - Video playback with motion markers

### Testing Tools
- Browser DevTools console
- cURL commands
- JavaScript test suite
- Load testing scripts

## 📊 System Requirements

### Hardware
- **Device**: Luckfox Pico RV1106
- **RAM**: 256MB (150MB available for web server)
- **Storage**: 256MB Nand Flash (150MB available)

### Software
- **Web Server**: Mongoose (lightweight C-based)
- **Database**: SQLite
- **Video**: HLS (HTTP Live Streaming)
- **Stream**: MJPEG

### Browser Support
- Chrome 60+
- Firefox 55+
- Safari 12+
- Edge 79+

## 🔍 Troubleshooting Index

### Common Issues
| Issue | Solution | Documentation |
|-------|----------|---------------|
| Can't access interface | Check IP and connectivity | [Quick Start](QUICKSTART.md#troubleshooting) |
| Live stream not working | Check camera and device status | [Quick Start](QUICKSTART.md#troubleshooting) |
| No motion markers | Verify API and data | [API Testing](API_TESTING.md#response-validation) |
| Calendar not loading | Check motion counts API | [API Testing](API_TESTING.md#motion-counts-validation) |

### Debug Tools
- Browser console logging
- Network monitoring
- API response validation
- Performance testing

## 📈 Performance Guidelines

### Optimization Tips
- Use wired connection for video
- Close unnecessary browser tabs
- Clear browser cache regularly
- Monitor API response times

### Monitoring
- Check API response times
- Monitor memory usage
- Track video playback performance
- Validate data integrity

## 🔒 Security Notes

### Network Security
- Local network access only
- No internet connectivity required
- CORS configured for local access
- Input validation on all APIs

### Best Practices
- Keep device firmware updated
- Monitor access logs
- Validate API responses
- Handle errors gracefully

## 📞 Support Resources

### Self-Help
1. Check documentation
2. Use troubleshooting guides
3. Test API endpoints
4. Review error logs

### Development Support
- API testing tools
- Debug logging
- Performance monitoring
- Error handling examples

---

**Documentation Version:** 2.0.0  
**Last Updated:** 2025-07-26  
**Maintained By:** MotionSense Development Team 